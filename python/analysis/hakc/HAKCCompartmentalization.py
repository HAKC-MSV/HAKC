import bisect
import networkx as nx


class HAKCCompartmentalization:
    static_weights = ["type_diff_count", "direct_call_count",
                      "indirect_call_count", "same_origin_of_indirect_calls",
                      "accessed_globals"]
    dynamic_weights = ["func_called", "shared_mem", "mem_loads", "mem_stores"]

    other_weights = ["kernel_edge"]

    def __init__(self):
        self.compartment_topo = nx.DiGraph()
        self.use_symbols = True
        self.use_same_origin = not self.use_symbols
        self.dynamic_files = set()

    def __len__(self):
        return len(self.compartment_topo)

    def uses_symbols(self) -> bool:
        return self.use_symbols

    def valid_weight(self, weight_name: str) -> bool:
        all_weights = (*HAKCCompartmentalization.static_weights,
                       *HAKCCompartmentalization.dynamic_weights,
                       *HAKCCompartmentalization.other_weights)
        return weight_name in all_weights

    def set_use_symbols(self, use_symbols: bool):
        self.use_symbols = use_symbols
        self.use_same_origin = not use_symbols

    def uses_same_origin(self) -> bool:
        return self.use_same_origin

    def set_use_same_origin(self, use_same_origin: bool):
        self.set_use_symbols(not use_same_origin)

    def add_compartment(self, compartment):
        if compartment is None:
            raise RuntimeError("Compartment is None")
        self.compartment_topo.add_node(compartment)

    def merge_compartments(self, compartment_a, compartment_b, edge_capacity_func, **func_args):
        compartment_a.merge_compartment_data(compartment_b)

        # Move edges from compartment_b to compartment_a
        for neighbor in [n for n in self.compartment_topo.nodes if n != compartment_a and
                                                                   self.compartment_topo.has_edge(n, compartment_b)]:
            if neighbor.get_compartment_id() == 4247:
                print("Found it")
            if self.compartment_topo.has_edge(neighbor, compartment_a):
                original_edge_data = self.compartment_topo.get_edge_data(neighbor, compartment_a)
            edge_data = edge_capacity_func(neighbor, compartment_a, **func_args)
            self.compartment_topo.add_edge(neighbor, compartment_a, **edge_data)

        self.compartment_topo.remove_node(compartment_b)

    def _check_edge(self, head, tail):
        if head is None:
            raise RuntimeError("head is None")
        if tail is None:
            raise RuntimeError("tail is None")

        if head not in self.compartment_topo:
            raise RuntimeError("Head is not in compartmentalization")
        if tail not in self.compartment_topo:
            raise RuntimeError("Tail is not in compartmentalization")

    def add_compartment_transition(self, head, tail, **weights):
        if sum(weights.values()) == 0:
            return
        for weight_name in weights.keys():
            if not self.valid_weight(weight_name):
                raise RuntimeError(f"Invalid weight: {weight_name}")
        self._check_edge(head, tail)
        self.compartment_topo.add_edge(head, tail, **weights)

    def remove_compartment_transition(self, head, tail):
        self._check_edge(head, tail)
        self.compartment_topo.remove_edge(head, tail)

    def get_compartment_transition(self, head, tail):
        if head not in self.compartment_topo or tail not in self.compartment_topo:
            return None
        edge = self.compartment_topo.get_edge_data(head, tail)
        return edge

    def get_compartments(self):
        return self.compartment_topo.nodes

    def get_compartment(self, compartment_id: int):
        for compartment in self.get_compartments():
            if compartment.get_compartment_id() == compartment_id:
                return compartment
        return None

    def set_compartmentalization(self, compartment_topo: nx.DiGraph):
        self.compartment_topo = compartment_topo

    def set_dynamic_files(self, files):
        self.dynamic_files = files

    def get_dynamic_files(self):
        return self.dynamic_files

    def _compute_capacity(self, edge_attrs):
        type_diff_count = 0
        direct_call_count = 0
        indirect_call_count = 0
        same_origin_of_indirect_calls = 0
        accessed_globals = 0
        if "type_diff_count" in edge_attrs:
            type_diff_count = int(edge_attrs['type_diff_count'])
        if 'direct_call_count' in edge_attrs:
            direct_call_count = int(edge_attrs['direct_call_count'])
        if 'indirect_call_count' in edge_attrs:
            indirect_call_count = int(edge_attrs['indirect_call_count'])
        if 'same_origin_of_indirect_calls' in edge_attrs:
            same_origin_of_indirect_calls = int(edge_attrs['same_origin_of_indirect_calls'])
        if 'accessed_globals' in edge_attrs:
            accessed_globals = int(edge_attrs['accessed_globals'])

        if self.use_symbols:
            capacity_to_use = type_diff_count * (direct_call_count + indirect_call_count)
        elif self.use_same_origin:
            capacity_to_use = type_diff_count * (direct_call_count + same_origin_of_indirect_calls)
        else:
            capacity_to_use = type_diff_count

        if capacity_to_use == 0 and (direct_call_count > 0 or accessed_globals > 0):
            capacity_to_use = direct_call_count + accessed_globals
        else:
            capacity_to_use += accessed_globals

        return capacity_to_use

    def add_weight_edges(self,
                         weight_edge_name="weight"):
        for (u, v, edge_attrs) in self.compartment_topo.edges.data():
            weight = self._compute_capacity(edge_attrs)
            new_attr = dict()
            new_attr[weight_edge_name] = weight
            self.compartment_topo.add_edge(u, v, **new_attr)

    def get_kernel_compartment(self):
        kernel_compartment = self.get_compartment(0)
        if kernel_compartment is None:
            kernel_compartment = hakc.HAKCCompartment.HAKCCompartment()
            kernel_compartment.set_compartment_id(0)
            self.add_compartment(kernel_compartment)
        return kernel_compartment

    def add_to_kernel_compartment(self, compartment, edge_capcity_func, **func_args):
        kernel_compartment = self.get_kernel_compartment()
        self.merge_compartments(kernel_compartment, compartment, edge_capcity_func, **func_args)

    def to_yaml(self) -> dict:
        result = dict()

        result['COMPARTMENTS'] = list()
        result['FILES'] = list()
        for compartment in sorted(self.compartment_topo.nodes, key=lambda x: int(x.get_compartment_id())):
            compartment_yaml, files_yaml = compartment.to_yaml()
            if len(compartment_yaml) > 0:
                for neighbor, edge_weights in \
                        self.compartment_topo.adj[compartment].items():
                    if sum(edge_weights.values()) == 0:
                        continue
                    if 'TARGETS' not in compartment_yaml:
                        compartment_yaml['TARGETS'] = list()
                    bisect.insort(compartment_yaml['TARGETS'], neighbor.get_compartment_id())
                result['COMPARTMENTS'].append(compartment_yaml)
            if len(files_yaml) > 0:
                for file_yaml in files_yaml:
                    result['FILES'].append(file_yaml)
        return result

    def merge_compartmentalization(self, compartmentalization):
        for new_u, new_v, new_weights in \
                compartmentalization.compartment_topo.edges.data():
            found_head = None
            found_tail = None
            for compartment in self.get_compartments():
                if found_head is None:
                    for atom in new_u.get_all_atoms():
                        if compartment.contains_atom(atom):
                            found_head = compartment
                            break
                if found_tail is None:
                    for atom in new_v.get_all_atoms():
                        if compartment.contains_atom(atom):
                            found_tail = compartment
                            break
                if found_head and found_tail:
                    break

            if found_head is None:
                print(f"Could not find head compartment containing "
                      f"{new_u.get_all_atoms()}")
                continue
            if found_tail is None:
                print(f"Could not find tail compartment containing "
                      f"{new_v.get_all_atoms()}")
                continue
            if found_head != found_tail:
                if self.compartment_topo.has_edge(found_head, found_tail):
                    existing_weights = self.compartment_topo.get_edge_data(
                        found_head, found_tail)
                    for weight_name, weight_value in new_weights.items():
                        if weight_name in existing_weights:
                            self.compartment_topo.edges[found_head, found_tail][weight_name] += weight_value
                        else:
                            self.compartment_topo.edges[found_head, found_tail][weight_name] = weight_value
                        print(f"{found_head.get_all_atoms()} -> "
                              f"{found_tail.get_all_atoms()}[{weight_name}] = "
                              f"{self.compartment_topo.edges[found_head, found_tail][weight_name]}")
                else:
                    print(f"!!! No edge between {found_head.get_all_atoms()} "
                          f"and {found_tail.get_all_atoms()}")
                    self.compartment_topo.add_edge(found_head, found_tail,
                                                   **new_weights)
