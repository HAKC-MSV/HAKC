import argparse
import concurrent.futures
import itertools
import multiprocessing as mp
import os
import pickle
import re
import sys
import time
from enum import Enum

import networkx as nx
import yaml
from networkx.algorithms.connectivity import EdgeComponentAuxGraph

from hakc.HAKCCompartment import HAKCCompartment
from hakc.HAKCCompartmentalization import HAKCCompartmentalization
from hakc.StructInfo import StructInfo
from hakc.SymbolInfo import SymbolInfo

max_workers = mp.cpu_count() - 1
# max_workers = 1

types_to_filter = {
    "include/linux/spinlock_types.h:71",
    "include/linux/trace_events.h",
    "include/linux/tracepoint-defs.h"
    "include/linux/tracepoint.h",
    "include/linux/trace_seq.h",
    "kernel/trace/trace.h",
    "include/linux/ftrace.h",
    "kernel/trace/trace_probe.h",
    "kernel/trace/trace_dynevent.h",
    "arch/arm64/include/uapi/asm/ptrace.h",
    "include/linux/trace_events.h",
    "arch/arm64/include/asm/stacktrace.h",
    "kernel/trace/trace_entries.h",
    "include/linux/trace.h",
    "include/trace/events/",
    "include/linux/types.h:166",
    "include/linux/types.h:173",
    "include/linux/refcount.h:111",
    "include/linux/lockdep_types.h:187",
    "include/linux/cpumask.h:17"
}

ignore_outgoing_edges = {
    "kernel/printk/printk.c",
    "kernel/locking/spinlock.c"
}

debug_paths = {
    # "/home/de29664/code/MTE-kernel/net/ipv6/af_inet6.c",
    # "/home/de29664/code/MTE-kernel/net/ipv6/ndisc.c",
}

profile_code = False
current_compartmentalization = None


class WorkerResult:
    def __init__(self):
        self.head_compartment = None
        self.weight_map = dict()


class EdgeComputeType(Enum):
    StaticOnly = 0
    DynamicAndStatic = 1


def output_statistics(struct_list):
    shared_names = dict()
    type_map = dict()
    all_structs = set()
    shared_name_count = 0
    same_name_same_type_count = 0
    conflict_count = 0

    print("Gathering statistics...", end='')
    sys.stdout.flush()
    for _, structs in struct_list.items():
        for si in structs:
            if si.type_hash not in type_map:
                type_map[si.type_hash] = set()
            type_map[si.type_hash].add(si)
            all_structs.add(si)
            if re.match("^struct_[0-9]+$", si.name):
                continue
            if si.name not in shared_names:
                shared_names[si.name] = set()
            shared_names[si.name].add(si)

    print('done')
    for type_hash, infos in type_map.items():
        if len(infos) > 1:
            conflict_count += len(infos)
            print("---- Conflict Detected ----")
            for struct_info in infos:
                print("----")
                print(struct_info)
                print("^^^^")

    print("###############################################")

    for name, struct_infos in shared_names.items():
        if len(struct_infos) > 1:
            shared_name_count += len(struct_infos)
            print("---- Same Name ----")
            same_types = dict()
            for struct_info in struct_infos:
                print("----")
                print(struct_info)
                print("^^^^")
                if struct_info.type_hash not in same_types:
                    same_types[struct_info.type_hash] = set()
                same_types[struct_info.type_hash].add(struct_info)
            for type_hash, infos in same_types.items():
                if len(infos) > 1:
                    same_name_same_type_count += len(infos)
                    print("---- Same Name Same Type ----")
                    for struct_info in infos:
                        print("----")
                        print(struct_info)
                        print("^^^^")
                    print('============================')
            print('============================')

    print("Total Structs: {}".format(len(all_structs)))
    print("Total Conflicts: {}".format(conflict_count))
    print("Shared Name Count: {}".format(shared_name_count))
    print("Same Name Same Type Count: {}".format(same_name_same_type_count))


def is_type_filtered(ty: StructInfo) -> bool:
    if not ty.path:
        return True
    if ty.users is not None and len(ty.users) > 0:
        for f in types_to_filter:
            tokens = f.split(':')
            if ty.path.endswith(tokens[0]) and (len(tokens) == 1 or
                                                int(tokens[1]) == ty.line):
                return True
        return False
    else:
        return True


def member_matches_type(target_hash: str, hakc_type_hash: str, index: int,
                        all_types: set) -> bool:
    if index < 0:
        return False

    target_type = None
    for hakc_type in all_types:
        if hakc_type.get_type_hash() == hakc_type_hash:
            target_type = hakc_type
            break

    if target_type is None:
        return False

    previous_index = None
    for member_index, type_hash in target_type.get_members().items():
        if member_index == index:
            return type_hash == target_hash
        elif member_index > index:
            if previous_index is None:
                break
            return member_matches_type(target_hash,
                                       target_type.get_members()
                                       [previous_index],
                                       index - previous_index, all_types)
        previous_index = member_index
    return False


def compute_dag_edge_capacities(head_compartment: HAKCCompartment,
                                tail_compartment: HAKCCompartment,
                                filter_func,
                                **filter_func_args):
    print_debug = False
    head_sources = head_compartment.get_definition_sources()
    for debug_path in debug_paths:
        if debug_path in head_sources:
            print_debug = True
            break
    if head_compartment.get_compartment_id() == tail_compartment.get_compartment_id():
        return None
    cu_types = head_compartment.get_accessed_types()
    types_len = len(cu_types)

    direct_calls = set()
    indirect_calls = dict()
    global_variables = set()
    for head_compartment_symbol in head_compartment.get_accessed_symbols():
        if head_compartment_symbol.is_global_variable():
            global_variables.add(head_compartment_symbol)
        for direct_call in head_compartment_symbol.get_direct_calls():
            direct_calls.add(direct_call)
        for type_hash, origins in head_compartment_symbol.get_indirect_calls().items():
            if type_hash not in indirect_calls:
                indirect_calls[type_hash] = set()
            for origin in origins:
                indirect_calls[type_hash].add(origin)

    if print_debug:
        print(f"Globals for {head_compartment.get_compartment_id()}:")
        for g in global_variables:
            print(f"{g}")
        print(f"Types for {head_compartment.get_compartment_id()}:")
        for t in cu_types:
            print(str(t))
        print(f"Direct calls for {head_compartment.get_compartment_id()}")
        for dc in sorted(direct_calls):
            print(dc)
        print(f"Indirect calls for {head_compartment.get_compartment_id()}")
        for type_hash, origins in indirect_calls.items():
            print(f"{type_hash}:")
            for origin in origins:
                print(f"\t{origin}")

    type_diff = cu_types - tail_compartment.get_accessed_types()
    type_diff_count = types_len - len(type_diff)
    indirect_call_count = 0
    direct_call_count = 0
    same_origin_of_indirect_calls = 0
    accessed_globals = 0
    for compartment_symbol in tail_compartment.get_accessed_symbols():
        compartment_defined_symbol = tail_compartment.defines_symbol(
            compartment_symbol) and not head_compartment.defines_symbol(compartment_symbol)

        if print_debug:
            if compartment_defined_symbol:
                print(f'Compartment {tail_compartment.get_compartment_id()} defined {compartment_symbol}')
            else:
                print(f'Compartment {tail_compartment.get_compartment_id()} does not define\n{compartment_symbol}')

        if compartment_symbol.is_global_variable() and \
                compartment_defined_symbol and \
                compartment_symbol in global_variables:
            for global_variable in global_variables:
                if global_variable == compartment_symbol and global_variable.get_definition_site() == \
                        compartment_symbol.get_definition_site():
                    accessed_globals += 1

        if compartment_symbol.is_function() and \
                compartment_defined_symbol and \
                compartment_symbol.get_name() in direct_calls:
            direct_call_count += 1
            if print_debug:
                print(f"!! {compartment_symbol.get_name()} in compartment "
                      f"{tail_compartment.get_compartment_id()} is called"
                      f" in compartment {head_compartment.get_compartment_id()}")

        if compartment_symbol.is_function() and \
                compartment_defined_symbol and \
                compartment_symbol.get_hash() in indirect_calls and \
                len(compartment_symbol.get_escapes()) > 0:
            # This clique indirectly calls a function defined in the
            # compartment that has escaped (address taken)
            indirect_call_count += 1
            if print_debug:
                print(f"@@ {compartment_symbol.get_name()} "
                      f"({compartment_symbol.get_hash()}) "
                      f"in {tail_compartment.get_compartment_id()} is "
                      f"indirectly called from {head_compartment.get_compartment_id()}")
            for clique_origin in \
                    indirect_calls[compartment_symbol.get_hash()]:
                if clique_origin.get_offset() is None:
                    continue
                for compartment_escape in compartment_symbol.get_escapes():
                    if compartment_escape.get_offset() is None:
                        continue
                    for cu_type in cu_types:
                        if cu_type.get_type() != \
                                compartment_escape.get_type():
                            continue
                        # We have found the type that the function
                        # escapes to, so check if any share origins
                        indices_to_check = set()
                        if clique_origin.get_offset() < 0:
                            for escaping_member, _ in \
                                    cu_type.get_escaping_members().items():
                                index = escaping_member + \
                                        compartment_escape.get_offset() + \
                                        clique_origin.get_offset()
                                indices_to_check.add(index)
                                if print_debug:
                                    print(f"!! index = "
                                          f"{escaping_member} "
                                          f"+ {compartment_escape.get_offset()} "
                                          f"+ {clique_origin.get_offset()} "
                                          f"= {index}")
                        if len(indices_to_check) == 0:
                            indices_to_check.add(clique_origin.get_offset())
                            if print_debug:
                                print(f"!! index = "
                                      f"{clique_origin.get_offset()}")
                        for index in indices_to_check:
                            if member_matches_type(
                                    compartment_symbol.get_hash(),
                                    cu_type.get_type(),
                                    index,
                                    cu_types):
                                if print_debug:
                                    print(f"## Same origin {index}")
                                same_origin_of_indirect_calls += 1

    if same_origin_of_indirect_calls > indirect_call_count:
        same_origin_of_indirect_calls = indirect_call_count

    edge_data = dict(type_diff_count=type_diff_count,
                     direct_call_count=direct_call_count,
                     indirect_call_count=indirect_call_count,
                     same_origin_of_indirect_calls=same_origin_of_indirect_calls,
                     accessed_globals=accessed_globals)

    if filter_func:
        potential_edge_capacity = filter_func(*filter_func_args, **edge_data)
        if potential_edge_capacity <= 0:
            if print_debug:
                print(
                    f'potential edge capacity between {head_compartment.get_compartment_id()} -> '
                    f'{tail_compartment.get_compartment_id()} is {potential_edge_capacity}. No edge created.')
            return None
    if print_debug:
        print(f'Adding edge between {head_compartment.get_compartment_id()} '
              f'and {tail_compartment.get_compartment_id()}')

    if print_debug:
        print(f"Finished with {head_compartment.get_compartment_id()} -> {tail_compartment.get_compartment_id()}")
    return edge_data


def compute_capacity(use_symbols: bool, use_same_origin: bool,
                     type_diff_count: int, direct_call_count: int,
                     indirect_call_count: int,
                     same_origin_of_indirect_calls: int,
                     accessed_globals: int) -> int:
    if use_symbols:
        capacity_to_use = type_diff_count * (
                direct_call_count + indirect_call_count)
    elif use_same_origin:
        capacity_to_use = type_diff_count * \
                          (direct_call_count + same_origin_of_indirect_calls)
    else:
        capacity_to_use = type_diff_count

    if capacity_to_use == 0 and (direct_call_count > 0 or accessed_globals > 0):
        capacity_to_use = direct_call_count + accessed_globals

    return capacity_to_use


def handle_compartment(compartmentalization: HAKCCompartmentalization, compartment: HAKCCompartment):
    existing_compartment = compartmentalization.get_compartment(compartment.get_compartment_id())
    if existing_compartment:
        existing_compartment.merge_compartment_data(compartment)
    else:
        compartmentalization.add_compartment(compartment)


class CompartmentAnalysisDataIter:
    def __init__(self, filename_compartment_map, filter_types, filter_mod_files, use_symbols, use_same_origin):
        global compartments
        self.compartment_iter = itertools.product(compartments.keys(), compartments.keys())
        self.filename_compartment_map = filename_compartment_map
        self.filter_types = filter_types
        self.filter_mod_files = filter_mod_files
        self.use_symbols = use_symbols
        self.use_same_origin = use_same_origin
        self.total_tasks = 0

    def __iter__(self):
        return self

    def __next__(self):
        global compartments
        next_data = EdgeInitData()
        head_compartment_id, tail_compartment_id = next(self.compartment_iter)
        while head_compartment_id == tail_compartment_id:
            head_compartment_id, tail_compartment_id = next(self.compartment_iter)
        self.total_tasks += 1
        next_data.head_compartment_id = head_compartment_id
        next_data.tail_compartment_id = tail_compartment_id
        next_data.filter_types = self.filter_types
        next_data.filter_mod_files = self.filter_mod_files
        next_data.filter_func_args['use_symbols'] = self.use_symbols
        next_data.filter_func_args['use_same_origin'] = self.use_same_origin
        return next_data


class CompartmentCreate:
    def __init__(self, path, compartment_id):
        self.path = path
        self.compartment_id = compartment_id


def worker_create_compartment(data):
    compilation_unit, yml_data = process_yaml_file(data.path, True, True)
    if yml_data is None:
        return None
    return create_compartment(data.compartment_id, compilation_unit, yml_data)


def create_data_access_graph(root_path, filter_types, filter_mod_files, outpath,
                             use_symbols=True, use_same_origin=False, ignore_outgoing=False):
    print(f"Creating graph (use_symbols = {use_symbols} ignore_outgoing = "
          f"{ignore_outgoing})...", end='', flush=True)
    compartmentalization = HAKCCompartmentalization()
    compartmentalization.set_use_symbols(use_symbols)
    compartmentalization.set_use_same_origin(use_same_origin)
    filename_compartment_map = dict()
    compartment_id = 1
    for root, subdirs, files in os.walk(root_path):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            filename_compartment_map[filename] = compartment_id
            compartment_id += 1

    print(f'Done.')
    total_edges_added = 0
    completed_tasks = 0
    start = time.time()
    tasks_per_process = 1000
    time_since_printing = start
    max_rounds = 5
    global compartments
    print(f"Creating compartments...", end='', flush=True)
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        for compartment in executor.map(worker_create_compartment, [CompartmentCreate(path, cid) for path,
        cid in filename_compartment_map.items()]):
            if compartment:
                compartments[compartment.get_compartment_id()] = compartment
                compartmentalization.add_compartment(compartment)
    print(f"Done. Created {len(compartmentalization.get_compartments())} compartments.")

    total_tasks = len(compartments) ** 2 - len(compartments)
    data_tracker = CompartmentAnalysisDataIter(filename_compartment_map,
                                               filter_types, filter_mod_files,
                                               use_symbols, use_same_origin)
    it = iter(data_tracker)
    while data_tracker.total_tasks < total_tasks:
        rounds = 0
        print(f"Starting pool")
        with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
            while rounds <= max_rounds:
                rounds += 1
                print(f"Starting edge computation tasks...")

                for head_compartment_id, tail_compartment_id, edge_weights in \
                        executor.map(analyze_compartments, itertools.islice(it, max_workers * tasks_per_process),
                                     chunksize=tasks_per_process):
                    try:
                        completed_tasks += 1
                        if edge_weights:
                            head_compartment = compartments[head_compartment_id]
                            tail_compartment = compartments[tail_compartment_id]
                            compartmentalization.add_compartment_transition(head_compartment,
                                                                            tail_compartment,
                                                                            **edge_weights)
                            total_edges_added += 1

                        now = time.time()
                        delay_time = now - time_since_printing
                        if delay_time >= 2:
                            print(f"Completed {completed_tasks} out of {total_tasks} tasks and added"
                                  f" {total_edges_added} edges")
                            time_since_printing = now
                    except Exception as e:
                        print(e)
                        raise e
    time_diff = time.time() - start
    print(f"{completed_tasks} Type difference calculations Done! It took {time_diff} seconds to compute")
    print(f"Total edges added: {total_edges_added}")
    print(f"Writing graph to {outpath}...", end='', flush=True)
    with open(outpath, 'wb') as f:
        pickle.dump(compartmentalization, f)
    print("Done")
    return compartmentalization


class YamlResult:
    def __init__(self):
        self.struct_dict = dict()
        self.symbol_dict = dict()
        self.symbol_name_map = dict()

    @staticmethod
    def _create_list(d: dict):
        result = list()
        for _, it in d.items():
            for i in it:
                result.append(i)
        return result

    @property
    def symbol_list(self):
        return self._create_list(self.symbol_dict)

    @property
    def struct_list(self):
        return self._create_list(self.struct_dict)

    def handle_symbol(self, symbol_info: SymbolInfo):
        append_to_symbol_list = True
        if symbol_info in self.symbol_dict:
            for existing_symbol in self.symbol_dict[symbol_info]:
                # Ensure two symbols with the same name and type from two different compilation units are treated
                # separately. This can happen if two globals are declared static.
                if existing_symbol.get_definition_site() is None or symbol_info.get_definition_site() is None:
                    # We have matched name and type but different declaration locations. This symbol must refer
                    # to the same variable, but we may not know where the definition is. Merge all the information
                    # we know so far.
                    existing_symbol.merge_symbol_data(symbol_info)
                    append_to_symbol_list = False
                    break

        if append_to_symbol_list:
            if symbol_info not in self.symbol_dict:
                self.symbol_dict[symbol_info] = list()
                self.symbol_name_map[symbol_info.get_name()] = list()

            self.symbol_dict[symbol_info].append(symbol_info)
            self.symbol_name_map[symbol_info.get_name()].append(symbol_info)

    def handle_type(self, struct_info: 'StructInfo', filter_types):
        append_to_struct_list = True
        if struct_info in self.struct_dict:
            for existing_type in self.struct_dict[struct_info]:
                existing_type.merge_struct_info(struct_info)
                struct_info = existing_type
                append_to_struct_list = False
                break

        for user in struct_info.get_users():
            if user not in self.symbol_name_map:
                continue
            symbol_list = self.symbol_name_map[user]
            for symbol in symbol_list:
                symbol.add_type(struct_info)

        if append_to_struct_list and (not filter_types or not is_type_filtered(struct_info)):
            if struct_info not in self.struct_dict:
                self.struct_dict[struct_info] = list()
            self.struct_dict[struct_info].append(struct_info)


def process_yaml_file(filename, filter_mod_files, filter_types):
    result = YamlResult()
    with open(filename, 'r') as yml:
        data = yaml.safe_load(yml)
        if not data:
            raise RuntimeError(f"No data for {filename}")

    compilation_unit = data['CU']
    if filter_mod_files and compilation_unit.endswith(".mod.c"):
        return compilation_unit, None
    if data['symbols'] is not None:
        for symbol_info in data['symbols']:
            info = SymbolInfo.from_yaml(symbol_info, compilation_unit)
            result.handle_symbol(info)
    if data['types'] is not None:
        for struct_info in data['types']:
            info = StructInfo.from_yaml(struct_info, compilation_unit)
            result.handle_type(info, filter_types)
    return compilation_unit, result


class EdgeInitData:
    def __init__(self):
        self.head_compartment_id = None
        self.tail_compartment_id = None
        self.filter_mod_files = False
        self.filter_types = False
        self.filter_func_args = dict()


def create_compartment(compartment_id: int, compilation_unit: str, yaml_result: YamlResult):
    compartment = HAKCCompartment()
    compartment.set_compartment_id(compartment_id)
    for symbol in yaml_result.symbol_list:
        if symbol.get_definition_site() and compilation_unit in symbol.get_definition_site():
            compartment.add_defined_symbol(symbol)
        else:
            compartment.add_symbol_access(symbol)
    for ty in yaml_result.struct_list:
        compartment.add_type_access(ty)
    return compartment


class CreateCompartmentInitData:
    def __init__(self):
        self.path_map = dict()
        self.filter_types = False
        self.filter_mod_files = False
        self.yaml_result = None


def create_compartments(init_data: CreateCompartmentInitData):
    result = dict()
    for path, compartment_id in init_data.path_map.items():
        compilation_unit, yml_data = process_yaml_file(path, init_data.filter_mod_files, init_data.filter_types)
        if yml_data is None:
            continue
        result[compartment_id] = create_compartment(compartment_id, compilation_unit, yml_data)
    return result


compartments = dict()


def analyze_compartments(init_data: EdgeInitData):
    global compartments
    # if len(compartments) == 0:
    #     create_compartment_data = CreateCompartmentInitData()
    #     create_compartment_data.path_map = init_data.filename_compartment_map
    #     create_compartment_data.filter_mod_files = init_data.filter_mod_files
    #     create_compartment_data.filter_types = init_data.filter_types
    #     compartments = create_compartments(create_compartment_data)

    head_compartment = compartments[init_data.head_compartment_id]
    tail_compartment = compartments[init_data.tail_compartment_id]

    edge_weights = compute_dag_edge_capacities(head_compartment, tail_compartment, compute_capacity,
                                               **init_data.filter_func_args)
    return head_compartment.get_compartment_id(), tail_compartment.get_compartment_id(), edge_weights


def compute_min_k_cut(compartmentalization: HAKCCompartmentalization,
                      k: int) -> None:
    begin = time.time()
    for i in range(k):
        print("Starting loop {}".format(i + 1))
        start = time.time()
        for scc in nx.strongly_connected_components(
                compartmentalization.compartment_topo):
            if len(scc) == 1:
                continue
            scc_subgraph = compartmentalization.compartment_topo.subgraph(scc)
            edges_to_remove = nx.minimum_edge_cut(scc_subgraph)
            for (u, v) in edges_to_remove:
                compartmentalization.compartment_topo.remove_edge(u, v)
        end = time.time()
        print("Elaped time: {}".format(end - start))
        with open('{}-compart.bin'.format(k), 'wb') as f:
            pickle.dump(compartmentalization, f)
    print("Total elapsed time: {}".format(time.time() - begin))


def construct_aux_graph(compartmentalization: HAKCCompartmentalization,
                        store_location: str) -> None:
    begin = time.time()
    print(f"Starting AUX graph construction to store at {store_location}")
    aux_graph = EdgeComponentAuxGraph.construct(
        compartmentalization.compartment_topo)
    print(f"Elapsed time: {time.time() - begin}")
    print(f"Writing aux_graph to {store_location}", end='...', flush=True)
    with open(store_location, 'wb') as f:
        pickle.dump(aux_graph, f)
    print("Done")


def partition_with_aux_graph(compartmentalization: HAKCCompartmentalization,
                             aux_graph: EdgeComponentAuxGraph):
    begin = time.time()
    max_size = 16
    curr_max = max_size + 1
    k = 1
    last_set = None
    while curr_max > max_size:
        count = 0
        loop_start = time.time()
        curr_max = 0
        non_unitary_subgraphs = 0
        for subgraph in aux_graph.k_edge_subgraphs(k):
            count += 1
            if len(subgraph) == 1:
                continue
            non_unitary_subgraphs += 1
            print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
            print("{} {} edge connected subgraph with length {}".format(
                count, k, len(subgraph)))
            if len(subgraph) > curr_max:
                if len(subgraph) > 1:
                    last_set = subgraph
                curr_max = len(subgraph)
            for compartment in subgraph:
                print(compartment)
            print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
        print("Total {} edge connected subgraphs: {}".format(k, count))
        print("Total {} edge connected non unitary subgraphs: {}".format(k,
                                                                         non_unitary_subgraphs))
        print("Elapsed time: {}".format(time.time() - loop_start))
        print("curr_max = {}".format(curr_max))
        k += 1
    print("Total Elapsed Time: {}".format(time.time() - begin))


def compute_incident_edge_weight(data_access_graph: nx.Graph) -> dict:
    result = dict()
    kernel_compartment = None
    for (u, v, capacity) in data_access_graph.edges(data='capacity'):
        if u not in result:
            result[u] = 0
        result[u] += capacity
        if v not in result:
            result[v] = 0
        result[v] += capacity
        if kernel_compartment is None:
            if u.get_compartment_id() == 0:
                kernel_compartment = u
            elif v.get_compartment_id() == 0:
                kernel_compartment = v

    if kernel_compartment is not None:
        result[kernel_compartment] = -1

    return result


def compute_static_weight(**weights) -> float:
    static_weight = 0.0
    if "type_diff_count" in weights:
        static_weight = float(weights["type_diff_count"])
    if "same_origin_of_indirect_calls" in weights:
        static_weight *= weights["same_origin_of_indirect_calls"]
    if "direct_call_count" in weights:
        static_weight += weights["direct_call_count"]

    return static_weight


def compute_dynamic_weight(**weights) -> float:
    dynamic_weight = 0.0
    for weight_name in HAKCCompartmentalization.dynamic_weights:
        if weight_name in weights:
            dynamic_weight += weights[weight_name]
    return dynamic_weight


def compute_edge_weight(edge_compute_type: EdgeComputeType,
                        total_static_weight: int,
                        total_dynamic_weight: int,
                        max_static_weight: int,
                        max_dynamic_weight: int,
                        **weights) -> float:
    result = 0.0
    if edge_compute_type == EdgeComputeType.StaticOnly:
        result = compute_static_weight(**weights) / float(max_static_weight)
    elif edge_compute_type == EdgeComputeType.DynamicAndStatic:
        dynamic_weight = compute_dynamic_weight(**weights)
        if dynamic_weight == 0.0:
            static_weight = compute_static_weight(**weights)
            if static_weight > 0.0:
                result = 1.0 / float(max_dynamic_weight)
        else:
            result = compute_dynamic_weight(**weights) / \
                     float(max_dynamic_weight)

    return result


def compute_total_static_weight(data_access_graph: nx.Graph) -> (int, int):
    static_weight = 0
    max_static_weight = 0
    for u, v, weights in data_access_graph.edges.data():
        weight = compute_static_weight(**weights)
        if weight > max_static_weight:
            max_static_weight = weight
        static_weight += weight
    return static_weight, max_static_weight


def compute_total_dynamic_weight(data_access_graph: nx.Graph) -> (int, int):
    dynamic_weight = 0
    max_dynamic_weight = 0
    for u, v, weights in data_access_graph.edges.data():
        weight = compute_dynamic_weight(**weights)
        if weight > max_dynamic_weight:
            max_dynamic_weight = weight
        dynamic_weight += weight
    return dynamic_weight, max_dynamic_weight


def greedy_dag_partition(data_access_graph: nx.Graph,
                         number_of_compartments: int,
                         edge_compute_type: EdgeComputeType) -> \
        HAKCCompartmentalization:
    compartmentalized_graph = HAKCCompartmentalization()
    compartment_id = 1

    clique_map = dict()
    print("Computing total static weight")
    all_static_weight, max_static_weight = compute_total_static_weight(
        data_access_graph)
    print(f"{all_static_weight}\nComputing total dynamic weight")
    all_dynamic_weight, max_dynamic_weight = compute_total_dynamic_weight(
        data_access_graph)
    print(f"{all_dynamic_weight}")
    print(f"Sorting {len(data_access_graph.edges)} edges")
    sorted_edges = sorted(data_access_graph.edges.data(), reverse=True,
                          key=lambda x, ect=edge_compute_type,
                                     sw=all_static_weight,
                                     dw=all_dynamic_weight,
                                     mx_s=max_static_weight,
                                     mx_d=max_dynamic_weight
                          : compute_edge_weight(ect, sw, dw, mx_s, mx_d, **x[2]))
    print("Done!")
    current_compartment = None
    for edge in sorted_edges:
        head = edge[0]
        tail = edge[1]
        weights = edge[2]
        # There are four possible scenarios:
        # 1. head and tail are not in a compartment
        # 2. head and tail are in different compartments
        # 3. head xor tail are not in a compartment
        # 4. head and tail are in the same compartment
        head_clique = None
        head_atom = None
        tail_clique = None
        tail_atom = None
        for atom in head.get_all_atoms():
            if head_atom:
                raise RuntimeError(f"There should only be one "
                                   f"clique in a DAG compartment cliques: "
                                   f"{head.get_all_atoms()}")
            head_atom = atom
        for atom in tail.get_all_atoms():
            if tail_atom:
                raise RuntimeError("There should only be one "
                                   "clique in a DAG compartment")
            tail_atom = atom

        if head_atom in clique_map:
            head_clique = clique_map[head_atom]

        if tail_atom in clique_map:
            tail_clique = clique_map[tail_atom]

        if head_clique is None and tail_clique is None:
            # Scenario 1
            if len(compartmentalized_graph) < number_of_compartments:
                print(f'Creating new compartment {compartment_id}')
                current_compartment = HAKCCompartment()
                current_compartment.set_compartment_id(compartment_id)
                compartment_id += 1
                compartmentalized_graph.add_compartment(current_compartment)
            else:
                if compartment_id > number_of_compartments:
                    compartment_id = 1
                current_compartment = compartmentalized_graph.get_compartment(
                    compartment_id)
                compartment_id += 1

            for clique in head.get_cliques().nodes:
                # There should only be one clique in a DAG compartment
                if head_clique:
                    raise RuntimeError("There should only be one "
                                       "clique in a DAG compartment")
                head_clique = clique.copy()
                head_clique.set_compartment(None)
            for clique in tail.get_cliques().nodes:
                # There should only be one clique in a DAG compartment
                if tail_clique:
                    raise RuntimeError("There should only be one "
                                       "clique in a DAG compartment")
                tail_clique = clique.copy()
                tail_clique.set_compartment(None)
            print(f'Adding {head_clique.get_atom()} and '
                  f'{tail_clique.get_atom()} to compartment '
                  f'{current_compartment.get_compartment_id()}')
            current_compartment.add_clique(head_clique)
            current_compartment.add_clique(tail_clique)
            clique_map[head_atom] = head_clique
            clique_map[tail_atom] = tail_clique
            if compute_edge_weight(edge_compute_type, all_static_weight,
                                   all_dynamic_weight, max_static_weight,
                                   max_dynamic_weight, **weights) \
                    > 0:
                current_compartment.add_edge(head_clique, tail_clique,
                                             **weights)
        elif head_clique and tail_clique:
            if head_clique.get_compartment_id() == \
                    tail_clique.get_compartment_id():
                # Scenario 4
                compartment = compartmentalized_graph.get_compartment(
                    head_clique.get_compartment_id())
                if compute_edge_weight(edge_compute_type, all_static_weight,
                                       all_dynamic_weight, max_static_weight,
                                       max_dynamic_weight, **weights) > 0:
                    print(f'Adding edge between {head_clique.get_atom()} and '
                          f'{tail_clique.get_atom()} in compartment '
                          f'{compartment.get_compartment_id()}')
                    compartment.add_edge(head_clique, tail_clique, **weights)
            else:
                # Scenario 2
                head_compartment = compartmentalized_graph.get_compartment(
                    head_clique.get_compartment_id())
                tail_compartment = compartmentalized_graph.get_compartment(
                    tail_clique.get_compartment_id())
                if compute_edge_weight(edge_compute_type, all_static_weight,
                                       all_dynamic_weight, max_static_weight,
                                       max_dynamic_weight, **weights) > 0:
                    compartmentalized_graph.add_compartment_transition(
                        head_compartment, tail_compartment, **weights)
                    print(f'Adding compartment transition between '
                          f'{head_compartment.get_compartment_id()} and '
                          f'{tail_compartment.get_compartment_id()} because of '
                          f'{head_clique.get_atom()} - {tail_clique.get_atom()} '
                          f'edge')
        else:
            # Scenario 3
            compartment = None
            new_clique = None
            created_new_compartment = False
            if len(compartmentalized_graph) < number_of_compartments:
                print(f'Creating new compartment {compartment_id}')
                current_compartment = HAKCCompartment()
                current_compartment.set_compartment_id(compartment_id)
                compartment_id += 1
                compartmentalized_graph.add_compartment(current_compartment)
                created_new_compartment = True
            else:
                if compartment_id > number_of_compartments:
                    compartment_id = 1
                current_compartment = compartmentalized_graph.get_compartment(
                    compartment_id)
                compartment_id += 1
            if head_clique:
                compartment = compartmentalized_graph.get_compartment(
                    head_clique.get_compartment_id())
                for clique in tail.get_cliques().nodes:
                    # There should only be one clique in a DAG compartment
                    if new_clique:
                        raise RuntimeError("There should only be one "
                                           "clique in a DAG compartment")
                    new_clique = clique.copy()
                    new_clique.set_compartment(None)
                clique_map[tail_atom] = new_clique
            else:
                compartment = compartmentalized_graph.get_compartment(
                    tail_clique.get_compartment_id())
                for clique in head.get_cliques().nodes:
                    # There should only be one clique in a DAG compartment
                    if new_clique:
                        raise RuntimeError("There should only be one "
                                           "clique in a DAG compartment")
                    new_clique = clique.copy()
                    new_clique.set_compartment(None)
                clique_map[head_atom] = new_clique
            if created_new_compartment:
                print(f'Adding {new_clique.get_atom()} to compartment '
                      f'{current_compartment.get_compartment_id()}')
                current_compartment.add_clique(new_clique)
                if compute_edge_weight(edge_compute_type, all_static_weight,
                                       all_dynamic_weight, max_static_weight,
                                       max_dynamic_weight, **weights) > 0:
                    if head_clique:
                        print(f'Adding compartment transition between '
                              f'{compartment.get_compartment_id()} and '
                              f'{current_compartment.get_compartment_id()}')
                        compartmentalized_graph.add_compartment_transition(
                            compartment, current_compartment, **weights)
                    else:
                        print(f'Adding compartment transition between '
                              f'{current_compartment.get_compartment_id()} and '
                              f'{compartment.get_compartment_id()}')
                        compartmentalized_graph.add_compartment_transition(
                            current_compartment, compartment, **weights)
            else:
                print(f'Adding new clique {new_clique.get_atom()} to '
                      f'compartment {compartment.get_compartment_id()}')
                compartment.add_clique(new_clique)
                if compute_edge_weight(edge_compute_type, all_static_weight,
                                       all_dynamic_weight, max_static_weight,
                                       max_dynamic_weight, **weights) > 0:
                    if head_clique:
                        print(f'Adding edge between {head_clique.get_atom()} and '
                              f'{new_clique.get_atom()}')
                        compartment.add_edge(head_clique, new_clique, **weights)
                    else:
                        print(f'Adding edge between {new_clique.get_atom()} and '
                              f'{tail_clique.get_atom()}')
                        compartment.add_edge(new_clique, tail_clique, **weights)
    return compartmentalized_graph


def find_or_make_kernel_compartment(compartmentalization: HAKCCompartmentalization) -> HAKCCompartment:
    for compartment in compartmentalization.get_compartments():
        if compartment.get_compartment_id() == 0:
            return compartment
    kernel_compartment = HAKCCompartment()
    kernel_compartment.set_compartment_id(0)
    compartmentalization.add_compartment(kernel_compartment)
    return kernel_compartment


# This code assumes that the compartmentalization is the initial data-access-graph
def compartmentalize_modules(compartmentalization: HAKCCompartmentalization,
                             build_path: str) -> HAKCCompartmentalization:
    mod_files = list()
    for root, _, files in os.walk(build_path):
        for f in files:
            _, ext = os.path.splitext(f)
            if ext == ".mod":
                mod_files.append(os.path.join(root, f))

    max_id = max(compartmentalization.get_compartments(),
                 key=lambda compartment: compartment.get_compartment_id()).get_compartment_id()

    print(f"max_id = {max_id}")
    compartment_id = max_id + 1
    compartments = dict()
    func_args = {"filter_func": compute_capacity,
                 "use_symbols": compartmentalization.uses_symbols(),
                 "use_same_origin": compartmentalization.uses_same_origin()}
    for mod_file in mod_files:
        with open(mod_file, 'r') as f:
            for obj_name in f.readline().split():
                source_name, _ = os.path.splitext(obj_name)
                # Don't add extension to handle all kinds of files
                source_name += "."
                if source_name in compartments and compartments[source_name] != compartment_id:
                    raise RuntimeError(f"{source_name} is found in {compartments[source_name]} and {compartment_id}")
                compartments[source_name] = compartment_id

        compartment = HAKCCompartment()
        compartment.set_compartment_id(compartment_id)
        compartmentalization.add_compartment(compartment)
        compartment_id += 1

    for source_name, compartment_id in compartments.items():
        new_compartment = compartmentalization.get_compartment(compartment_id)
        old_compartment = None
        for compartment in compartmentalization.get_compartments():
            for atom in compartment.get_all_atoms():
                if atom.find(source_name) > -1:
                    old_compartment = compartment
                    break
            if old_compartment:
                break

        if old_compartment is None:
            print(f"Could not find old compartment {compartment_id - max_id} containing source_name {source_name}")
            continue
        if new_compartment is None:
            raise RuntimeError(f"Could not find new compartment {compartment_id}")

        print(f"Merging {old_compartment.get_compartment_id()} with {new_compartment.get_compartment_id()}")
        compartmentalization.merge_compartments(new_compartment, old_compartment, compute_dag_edge_capacities,
                                                **func_args)

    compartments_to_move_to_kernel = list()
    for compartment in compartmentalization.get_compartments():
        if compartment.get_compartment_id() > 0 and compartment.get_compartment_id() < max_id:
            compartments_to_move_to_kernel.append(compartment)

    for compartment in compartments_to_move_to_kernel:
        compartmentalization.add_to_kernel_compartment(compartment, compute_dag_edge_capacities, **func_args)

    return compartmentalization


def adjust_compartmentalization(compartmentalization: HAKCCompartmentalization,
                                adjustments) -> HAKCCompartmentalization:
    num_compartments = len(compartmentalization.compartment_topo)
    print(f"Adjusting {num_compartments} Compartments")
    if 'kernel' in adjustments:
        compartments_to_remove = set()
        compartments_to_keep = set()
        kernel_compartment = find_or_make_kernel_compartment(
            compartmentalization)
        for compartment in compartmentalization.get_compartments():
            if compartment.get_compartment_id() == kernel_compartment.get_compartment_id():
                continue
            if len(compartment.get_definition_sources()) == 0:
                compartments_to_remove.add(compartment)
                continue

            remove = True
            if 'compartmentalize' in adjustments:
                for compartmentalize_path in adjustments['compartmentalize']:
                    if compartment.contains_symbol_defined_in_path(compartmentalize_path):
                        remove = False
                        break

            if remove:
                compartments_to_remove.add(compartment)
            else:
                print(f"Not removing {compartment}")
                compartments_to_keep.add(compartment)

        print(f"Removing {len(compartments_to_remove)} compartments")
        func_args = {"filter_func": compute_capacity,
                     "use_symbols": compartmentalization.uses_symbols(),
                     "use_same_origin": compartmentalization.uses_same_origin()}
        for compartment in compartments_to_remove:
            kernel_compartment.merge_compartment_data(compartment)
            compartmentalization.compartment_topo.remove_node(compartment)
        for compartment in compartments_to_keep:
            for other_compartment in [c for c in compartmentalization.get_compartments() if c != compartment]:
                edge_data = compute_dag_edge_capacities(compartment, other_compartment, **func_args)
                if edge_data:
                    compartmentalization.add_compartment_transition(compartment, other_compartment, **edge_data)

    print(f"Done adjusting compartmentalization. Current compartmentalization has "
          f"{len(compartmentalization)} compartments.")
    return compartmentalization


# Taken from https://stackoverflow.com/a/70423579
class IndentingEmitter(yaml.emitter.Emitter):
    def increase_indent(self, flow=False, indentless=False):
        """Ensure that lists items are always indented."""
        return super().increase_indent(
            flow=False,
            indentless=False,
        )


class PrettyDumper(
    IndentingEmitter,
    yaml.serializer.Serializer,
    yaml.representer.Representer,
    yaml.resolver.Resolver,
):
    def __init__(
            self,
            stream,
            default_style=None,
            default_flow_style=False,
            canonical=None,
            indent=None,
            width=None,
            allow_unicode=None,
            line_break=None,
            encoding=None,
            explicit_start=None,
            explicit_end=None,
            version=None,
            tags=None,
            sort_keys=True,
    ):
        IndentingEmitter.__init__(
            self,
            stream,
            canonical=canonical,
            indent=indent,
            width=width,
            allow_unicode=allow_unicode,
            line_break=line_break,
        )
        yaml.serializer.Serializer.__init__(
            self,
            encoding=encoding,
            explicit_start=explicit_start,
            explicit_end=explicit_end,
            version=version,
            tags=tags,
        )
        yaml.representer.Representer.__init__(
            self,
            default_style=default_style,
            default_flow_style=default_flow_style,
            sort_keys=sort_keys,
        )
        yaml.resolver.Resolver.__init__(self)


def main():
    parser = argparse.ArgumentParser(description='Kernel Data Access Analysis')
    parser.add_argument('-r', '--root', help='Root directory to search')
    parser.add_argument('-o', '--output', help='Path to output data '
                                               'structures')
    parser.add_argument('-c', '--compartment',
                        help='Path to existing compartmentalization',
                        nargs=1)
    parser.add_argument('-a', '--aux', help='Path to Auxillary graph',
                        nargs=1)
    parser.add_argument('--dag', help='Generate a data access graph',
                        action='store_true')
    parser.add_argument('--use_symbols',
                        help='Use symbols along with types to generate DAG',
                        action='store_true')
    parser.add_argument('--use_same_origin',
                        help='Use function pointers with same origin along '
                             'with types to generate DAG',
                        action='store_true')
    parser.add_argument('--ignore_outgoing',
                        help='Ignore outgoing edges of listed files',
                        action='store_true')
    parser.add_argument('-n', '--node_count', help='The number of nodes to '
                                                   'place in '
                                                   'the data access graph',
                        type=int)
    parser.add_argument('--subsets', help='Find files that share type '
                                          'subsets', action='store_true')
    parser.add_argument('--filter_types', help='Filter types when reading in '
                                               'struct data',
                        action='store_true')
    parser.add_argument('--filter_mod_files', help='Filter files that end in '
                                                   '.mod.c',
                        action='store_true')
    parser.add_argument('--debug_one_path_capacity',
                        help='Print edge capacities for one '
                             'path', nargs=1)
    parser.add_argument('--compute_k_cut', metavar='kcut', nargs=1, type=int,
                        help='Compute a min k-cut of the compartmentalization')
    parser.add_argument('--construct_aux_graph', nargs=1,
                        help='Compute a min k-cut of the compartmentalization')
    parser.add_argument('--construct_aux', metavar='aux', nargs=1,
                        help='Construct graph from auxillary graph')
    parser.add_argument('--construct_greedy', metavar='greedy', nargs=1,
                        type=int,
                        help='Compute a compartmentalization using the greedy '
                             'algorithm that contains the specified number of '
                             'compartments')
    parser.add_argument('--use_dynamic', help='Use dynamic data',
                        action='store_true')
    parser.add_argument('--test_graph', metavar='testgraph', nargs=1,
                        help='Graph to use instead of compartmentalization')
    parser.add_argument("--output_compart", nargs=1,
                        help='Output compartmentalization yaml')
    parser.add_argument("--adjust", nargs=1, help='Adjust compartmentalization')
    parser.add_argument("--merge", nargs=1, help='Merge compartmentalizations')
    parser.add_argument("--compartmentalize_modules", nargs=1,
                        help='Compartmentalize all modules in a build directory')

    args = parser.parse_args()

    compartmentalization = None
    test_graph = None

    if args.test_graph:
        with open(args.test_graph[0], 'rb') as f:
            test_graph = pickle.load(f)

    if args.dag:
        if args.compartment is None:
            raise RuntimeError("Unspecified output for compartmentalization")

        compartmentalization = create_data_access_graph(args.root, args.filter_types,
                                                        args.filter_mod_files, args.compartment[0],
                                                        args.use_symbols, args.use_same_origin,
                                                        args.ignore_outgoing)

    if compartmentalization is None and args.compartment:
        with open(args.compartment[0], 'rb') as f:
            print("Reading in compartmentalization...", end='', flush=True)
            compartmentalization = pickle.load(f)
            print("Done!")

    if args.compute_k_cut and args.compute_k_cut[0] > 0:
        if args.compartment is None:
            raise RuntimeError(
                "Unspecified output for compartmentalization")
        compute_min_k_cut(compartmentalization, args.compute_k_cut[0])

    if args.construct_aux_graph:
        if args.compartment is None:
            raise RuntimeError(
                "Unspecified output for compartmentalization")
        construct_aux_graph(compartmentalization, args.construct_aux_graph[0])

    if args.aux:
        if args.compartment is None:
            raise RuntimeError(
                "Unspecified output for compartmentalization")
        with open(args.aux[0], 'rb') as f:
            aux_graph = pickle.load(f)
        partition_with_aux_graph(compartmentalization, aux_graph)

    if args.construct_greedy:
        if compartmentalization:
            graph_to_use = compartmentalization.compartment_topo
        if test_graph:
            graph_to_use = test_graph

        edge_compute_type = EdgeComputeType.StaticOnly
        if args.use_dynamic:
            edge_compute_type = EdgeComputeType.DynamicAndStatic

        compartmentalization = greedy_dag_partition(graph_to_use,
                                                    args.construct_greedy[0],
                                                    edge_compute_type)
        original_file_name, extension = os.path.splitext(args.compartment[0])
        final_file_name = f'{original_file_name}-greedy-' \
                          f'{args.construct_greedy[0]}-' \
                          f'{edge_compute_type.name}{extension}'
        with open(final_file_name, 'wb') as f:
            pickle.dump(compartmentalization, f)

    if args.compartmentalize_modules:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization provided")

        compartmentalization = compartmentalize_modules(compartmentalization,
                                                        args.compartmentalize_modules[0])

    if args.adjust:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization provided")
        with open(args.adjust[0], 'r') as f:
            adjustments = yaml.safe_load(f)
        compartmentalization = adjust_compartmentalization(
            compartmentalization, adjustments)
        adjusted_file_name, extension = os.path.splitext(args.compartment[0])
        adjusted_file_name += "-adjusted" + extension
        with open(adjusted_file_name, 'wb') as f:
            pickle.dump(compartmentalization, f)

    if args.merge:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization provided")
        with open(args.merge[0], 'rb') as f:
            new_dag = pickle.load(f)
        compartmentalization.merge_compartmentalization(new_dag)
        merged_file_name, extension = os.path.splitext(args.compartment[0])
        merged_file_name += "-merged" + extension
        with open(merged_file_name, 'wb') as f:
            pickle.dump(compartmentalization, f)

    if args.output_compart:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization provided")
        with open(args.output_compart[0], 'w') as f:
            print(f"Outputting YAML to {args.output_compart[0]}")
            yaml.dump(compartmentalization.to_yaml(), f, Dumper=PrettyDumper)


if __name__ == "__main__":
    main()
