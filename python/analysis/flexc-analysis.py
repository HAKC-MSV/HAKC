import argparse
import concurrent.futures
import logging
import os

import pandas as pd
import yaml
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDivision, HAKCSymbol, HAKCType

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


class EscalationObjectType:
    yaml_name = 'escalation-objects'

    def __init__(self, name):
        self.type_name = name
        self.subtype_name = None
        if "::" in self.type_name:
            type_tokens = self.type_name.split("::")
            self.type_name = type_tokens[0]
            self.subtype_name = type_tokens[1]

    def __hash__(self):
        return hash((self.type_name, self.subtype_name))

    def __eq__(self, other):
        if isinstance(other, EscalationObjectType):
            return self.type_name == other.type_name and self.subtype_name == other.subtype_name
        return False


class VulnerableSymbol:
    yaml_name = "vulnerable-symbols"

    def __init__(self, name):
        self.name = name

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other):
        if isinstance(other, VulnerableSymbol):
            return self.name == other.name
        return False


class FlexCAnalysisData:
    symbol_name_filters = ["_SCT_", "_SCK_", "_UNIQUE_ID_", ".str"]

    def __init__(self):
        self.escalation_types: set[EscalationObjectType] = set()
        self.vulnerable_symbols: set[VulnerableSymbol] = set()
        self.total_compartments = 0
        self.compartments_that_allow_escalation: set[int] = set()
        self.compartments_with_escalation_objects: set[int] = set()
        self.compartments_with_vulnerable_symbols: set[int] = set()
        self.compartment_symbol_map = dict()

    @staticmethod
    def output_list_information(analysis_output: dict, analysis_name: str, analysis_set: set):
        analysis_output[analysis_name] = dict()
        analysis_output[analysis_name]['size'] = len(analysis_set)
        analysis_output[analysis_name]['data'] = sorted(list(analysis_set)) if len(analysis_set) > 0 else []

    def write_analysis_output(self, f):
        analysis_output = dict()
        analysis_output['total-compartments'] = self.total_compartments

        self.output_list_information(analysis_output, 'allowable-escalation-compartments',
                                     self.compartments_that_allow_escalation)
        self.output_list_information(analysis_output, 'escalation-compartments',
                                     self.compartments_with_escalation_objects)
        self.output_list_information(analysis_output, 'vulnerable-compartments',
                                     self.compartments_with_vulnerable_symbols)

        analysis_output['compartmentalization-info'] = dict()
        for compartment_id, symbols in self.compartment_symbol_map.items():
            analysis_output['compartmentalization-info'][compartment_id] = dict()
            filtered_symbols_names = set()
            filtered_symbol_definition_files = set()
            for symbol in symbols:
                add_symbol = True
                for name_filter in FlexCAnalysisData.symbol_name_filters:
                    if name_filter in symbol.name:
                        add_symbol = False
                        break
                if add_symbol:
                    filtered_symbols_names.add(symbol.name)
                    if symbol.defining_file is not None:
                        filtered_symbol_definition_files.add(symbol.defining_file)

            self.output_list_information(analysis_output['compartmentalization-info'][compartment_id],
                                         'filtered-symbol-names', filtered_symbols_names)
            self.output_list_information(analysis_output['compartmentalization-info'][compartment_id],
                                         'filter-symbol-definition-files', filtered_symbol_definition_files)

        yaml.dump(analysis_output, f)


def parse_inputs(input_yaml_files: set[str]) -> FlexCAnalysisData:
    result = FlexCAnalysisData()

    for input_file in input_yaml_files:
        with open(input_file, 'r') as file:
            parsed_yaml = yaml.safe_load(file)
        if EscalationObjectType.yaml_name in parsed_yaml:
            for type_name in parsed_yaml[EscalationObjectType.yaml_name]:
                result.escalation_types.add(EscalationObjectType(type_name))
        if VulnerableSymbol.yaml_name in parsed_yaml:
            for vulnerable_symbol_name in parsed_yaml[VulnerableSymbol.yaml_name]:
                result.vulnerable_symbols.add(VulnerableSymbol(vulnerable_symbol_name))

    return result


def get_compartments_using_type(db: HAKCDatabase, type_name: str) -> list[int]:
    cmd = f"""
    MATCH (ty:{HAKCType.get_table_name()})<-[e]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
    WHERE contains(ty.{str(HAKCType.get_data_columns()[0])}, $type_name)
    RETURN DISTINCT c.{str(HAKCCompartment.get_primary_key())} AS CompartmentID
    """
    response = db.execute_prepared_stmt(cmd, type_name=type_name)
    data = response.get_as_df()

    compartment_ids = set()
    compartment_ids.update(data['CompartmentID'].tolist())

    return sorted(list(compartment_ids))


def get_escalation_type_compartments(db: HAKCDatabase, type_name: str) -> list[int]:
    compartment_ids = get_compartments_using_type(db, type_name)
    return compartment_ids


def multicore_get_escalation_type_compartments(db_dir, type_name: str):
    db = HAKCDatabase(db_dir, read_only=True)
    compartment_ids = get_escalation_type_compartments(db, type_name)
    db.close()
    return compartment_ids


def get_compartment_with_vulnerable_symbol(db, symbol):
    return db.get_division_id_compartment_id_from_symbol(symbol)


def multicore_get_compartment_with_vulnerable_symbol(db_dir, symbol):
    db = HAKCDatabase(db_dir, read_only=True)
    compartment_tuple = get_compartment_with_vulnerable_symbol(db, symbol)
    db.close()
    return compartment_tuple


def get_symbols_in_compartment(db, compartment_id):
    symbol_hashes = db.get_all_symbol_hashes_in_compartment(compartment_id)
    return db.get_symbol_by_hash(symbol_hashes)


def multicore_get_symbols_in_compartment(db_dir, compartment_id):
    db = HAKCDatabase(db_dir, read_only=True)
    symbols = get_symbols_in_compartment(db, compartment_id)
    db.close()
    return symbols


def perform_analysis(analysis_data: FlexCAnalysisData, db_dir: str, multicore: bool):
    core_count = 1
    db = HAKCDatabase(db_dir, read_only=True)
    if multicore:
        from multiprocessing import cpu_count
        core_count = cpu_count()

    try:
        if multicore:
            with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
                futures_to_data = {executor.submit(multicore_get_escalation_type_compartments, db_dir,
                                                   escalation_type.type_name): escalation_type.type_name for
                                   escalation_type in analysis_data.escalation_types}
                for future in concurrent.futures.as_completed(futures_to_data):
                    for compartment_id in future.result():
                        analysis_data.compartments_with_escalation_objects.add(compartment_id)
        else:
            for escalation_type in analysis_data.escalation_types:
                compartment_ids = get_compartments_using_type(db, escalation_type.type_name)
                for compartment_id in compartment_ids:
                    analysis_data.compartments_with_escalation_objects.add(compartment_id)

        vulnerable_symbol_names = [vulnerable_symbol.name for vulnerable_symbol in analysis_data.vulnerable_symbols]
        vulnerable_symbols = list()
        for vulnerable_symbol_name in vulnerable_symbol_names:
            for sym in db.get_symbols_by_name(vulnerable_symbol_name):
                vulnerable_symbols.append(sym)

        if multicore:
            with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
                futures_to_data = {
                    executor.submit(multicore_get_compartment_with_vulnerable_symbol, db_dir,
                                    vulnerable_symbol): vulnerable_symbol for
                    vulnerable_symbol in vulnerable_symbols}
                for future in concurrent.futures.as_completed(futures_to_data):
                    compartment_tuple = future.result()
                    if compartment_tuple is not None:
                        analysis_data.compartments_with_vulnerable_symbols.add(compartment_tuple[1].compartment_id)
        else:
            for vulnerable_symbol in vulnerable_symbols:
                compartment_tuple = get_compartment_with_vulnerable_symbol(db, vulnerable_symbol)
                if compartment_tuple:
                    analysis_data.compartments_with_vulnerable_symbols.add(compartment_tuple[1].compartment_id)

        compartments = db.get_all_compartments()
        analysis_data.total_compartments = len(compartments)
        for vulnerable_compartment in analysis_data.compartments_with_vulnerable_symbols:
            if vulnerable_compartment in analysis_data.compartments_with_escalation_objects:
                analysis_data.compartments_that_allow_escalation.add(vulnerable_compartment)

        if multicore:
            with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
                futures_to_data = {executor.submit(multicore_get_symbols_in_compartment, db_dir,
                                                   compartment.compartment_id): compartment.compartment_id for
                                   compartment
                                   in compartments}
                for future in concurrent.futures.as_completed(futures_to_data):
                    compartment_id = futures_to_data[future]
                    symbols = future.result()
                    analysis_data.compartment_symbol_map[compartment_id] = symbols
        else:
            for compartment in compartments:
                symbols = get_symbols_in_compartment(db, compartment.compartment_id)
                analysis_data.compartment_symbol_map[compartment.compartment_id] = symbols

    finally:
        db.close()


def perform_node_analysis(db_dir: str, analysis_output_path: str):
    db = HAKCDatabase(db_dir, read_only=True)
    try:
        cmd = f"""
        MATCH (:{HAKCSymbol.get_table_name()})-[:UsesSymbol]->(sym:{HAKCSymbol.get_table_name()})-[:IsType]->(ty:{HAKCType.get_table_name()})
        RETURN DISTINCT sym.Name AS Name, ty.DebugType AS DebugType, ty.LLVMType as LLVMType, COUNT {{ MATCH (:{HAKCSymbol.get_table_name()})-[:UsesSymbol]->(sym) }} AS UseCount
        ORDER BY UseCount DESC
        """
        response = db.execute_prepared_stmt(cmd)
        data = response.get_as_df()
        response.close()

        cmd = f"""
        MATCH (:{HAKCSymbol.get_table_name()})-[:IndirectCall]->(ty:{HAKCType.get_table_name()})<-[:IsType]-(sym:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT sym.Name AS Name, ty.DebugType AS DebugType, ty.LLVMType as LLVMType, COUNT {{ MATCH (:{HAKCSymbol.get_table_name()})-[:IndirectCall]->(ty:{HAKCType.get_table_name()})<-[:IsType]-(sym:{HAKCSymbol.get_table_name()}) }} AS UseCount
        ORDER BY UseCount DESC
        """
        response = db.execute_prepared_stmt(cmd)
        data2 = response.get_as_df()
        response.close()

        data = pd.merge(data, data2, on=['Name', 'DebugType', 'LLVMType'], how='outer', suffixes=('_df1', '_df2'))
        data['UseCount_df1'] = data['UseCount_df1'].fillna(0)
        data['UseCount_df2'] = data['UseCount_df2'].fillna(0)
        data['UseSum'] = data['UseCount_df1'] + data['UseCount_df2']
        data = data[['Name', 'DebugType', 'LLVMType', 'UseSum']]

        data = data.sort_values(by='UseSum', ascending=False)

        data.to_csv(analysis_output_path)
    finally:
        db.close()


def perform_type_analysis(db_dir: str, analysis_output_path: str):
    db = HAKCDatabase(db_dir, read_only=True)

    try:
        cmd = f"""
        MATCH (ty:{HAKCType.get_table_name()})<-[e:IsType]-(:{HAKCSymbol.get_table_name()})<-[:UsesSymbol]-(:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT ty.DebugType AS DebugType, ty.LLVMType AS LLVMType, COUNT {{ MATCH (ty:{HAKCType.get_table_name()})<-[:IsType]-(:{HAKCSymbol.get_table_name()})<-[:UsesSymbol]-(:{HAKCSymbol.get_table_name()}) }} AS IncomingCount
        ORDER BY IncomingCount DESC
        """
        response = db.execute_prepared_stmt(cmd)
        data = response.get_as_df()
        response.close()

        cmd = f"""
        MATCH (:{HAKCSymbol.get_table_name()})-[:IndirectCall]->(ty:{HAKCType.get_table_name()})
        RETURN DISTINCT ty.DebugType AS DebugType, ty.LLVMType AS LLVMType, COUNT {{ MATCH (:{HAKCSymbol.get_table_name()})-[:IndirectCall]->(ty:{HAKCType.get_table_name()}) }} AS IncomingCount
        ORDER BY IncomingCount DESC
        """
        response = db.execute_prepared_stmt(cmd)
        data2 = response.get_as_df()
        response.close()

        data = pd.merge(data, data2, on=['DebugType', 'LLVMType'], how='outer', suffixes=('_df1', '_df2'))
        data['IncomingCount_df1'] = data['IncomingCount_df1'].fillna(0)
        data['IncomingCount_df2'] = data['IncomingCount_df2'].fillna(0)
        data['IncomingSum'] = data['IncomingCount_df1'] + data['IncomingCount_df2']
        data = data[['DebugType', 'LLVMType', 'IncomingSum']]

        data = data.sort_values(by='IncomingSum', ascending=False)
        data.to_csv(analysis_output_path)
    finally:
        db.close()


def main():
    parser = argparse.ArgumentParser(description='FLEXC Compartment Analysis')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    parser.add_argument('--analysis-output', default='analysis-output.yml', help='File to write analysis output',
                        dest="analysis_output")
    parser.add_argument('--input', nargs='+', required=False, help="Input files")
    parser.add_argument('--multicore', dest='multicore', action='store_true', default=False)
    parser.add_argument('--type-analysis', dest='type_analysis', action='store_true', default=False)
    parser.add_argument('--vulnerability-analysis', dest='vulnerability_analysis', action='store_true', default=False)
    parser.add_argument('--node-analysis', dest='node_analysis', action='store_true', default=False)

    args = parser.parse_args()

    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    if not os.path.exists(os.path.dirname(args.analysis_output)):
        os.makedirs(os.path.dirname(args.analysis_output))

    if args.node_analysis:
        perform_node_analysis(args.db_dir, args.analysis_output)

    if args.type_analysis:
        perform_type_analysis(args.db_dir, args.analysis_output)

    if args.vulnerability_analysis:
        with open(args.analysis_output, 'w') as f:
            analysis_data = parse_inputs(args.input)
            perform_analysis(analysis_data, args.db_dir, args.multicore)
            analysis_data.write_analysis_output(f)


if __name__ == "__main__":
    main()
