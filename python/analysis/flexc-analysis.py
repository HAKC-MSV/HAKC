import argparse
import logging

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
    def __init__(self):
        self.escalation_types: set[EscalationObjectType] = set()
        self.vulnerable_symbols: set[VulnerableSymbol] = set()
        self.total_compartments = 0
        self.compartments_that_allow_escalation: set[int] = set()
        self.compartments_with_escalation_objects: set[int] = set()
        self.compartments_with_vulnerable_symbols: set[int] = set()

    def write_analysis_output(self, f):
        analysis_output = dict()
        analysis_output['total-compartments'] = self.total_compartments
        analysis_output['allowable-escalation-compartment-total'] = len(self.compartments_that_allow_escalation)
        analysis_output['escalation-compartment-total'] = len(self.compartments_with_escalation_objects)
        analysis_output['vulnerable-compartment-total'] = len(self.compartments_with_vulnerable_symbols)

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
    MATCH (ty:{HAKCType.get_table_name()})<-[e]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(c:{HAKCCompartment.get_table_name()})
    WHERE contains(ty.{str(HAKCType.get_data_columns()[0])}, $type_name)
    RETURN DISTINCT c.{str(HAKCCompartment.get_primary_key())} AS CompartmentID
    """
    response = db.execute_prepared_stmt(cmd, type_name=type_name)
    data = response.get_as_df()
    return sorted(data['CompartmentID'].tolist())


def perform_analysis(analysis_data: FlexCAnalysisData, db_dir: str):
    db = HAKCDatabase(db_dir, read_only=True)
    try:
        for escalation_type in analysis_data.escalation_types:
            compartment_ids = get_compartments_using_type(db, escalation_type.type_name)
            for compartment_id in compartment_ids:
                analysis_data.compartments_with_escalation_objects.add(compartment_id)

        for vulnerable_symbol in analysis_data.vulnerable_symbols:
            db_symbols = db.get_symbols_by_name(vulnerable_symbol.name)
            for db_symbol in db_symbols:
                compartment_tuple = db.get_division_id_compartment_id_from_symbol(db_symbol)
                if compartment_tuple:
                    analysis_data.compartments_with_vulnerable_symbols.add(compartment_tuple[1].compartment_id)

        compartments = db.get_all_compartments()
        analysis_data.total_compartments = len(compartments)
        for vulnerable_compartment in analysis_data.compartments_with_vulnerable_symbols:
            if vulnerable_compartment in analysis_data.compartments_with_escalation_objects:
                analysis_data.compartments_that_allow_escalation.add(vulnerable_compartment)
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
    parser.add_argument('--input', nargs='+', required=True, help="Input files")

    args = parser.parse_args()

    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    with open(args.analysis_output, 'w') as f:
        analysis_data = parse_inputs(args.input)
        perform_analysis(analysis_data, args.db_dir)
        analysis_data.write_analysis_output(f)


if __name__ == "__main__":
    main()
