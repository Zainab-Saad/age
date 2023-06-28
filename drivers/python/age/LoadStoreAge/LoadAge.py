# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import age, csv, json
from typing import Tuple

def get_contents_from_csv(file_path) -> Tuple[list, list]:
    file = open(file_path)
    csvreader = csv.reader(file)
    header = []
    header = next(csvreader)
    records = []
    for record in csvreader:
        records.append(record)
    return header, records

def make_map_csv(header: str) -> str:
    properties = ''
    for each_property in header:
        properties += each_property + ': %s, '
    return properties[0: len(properties)-2]


def make_map_json(d, prop, record) -> None:
    for k, v in d.items():
        if isinstance(v, dict):
            (prop.append(',' + k + ': {') 
                if prop[-1][-1] == '}' 
                else prop.append(k + ': {'))
            make_map_json(v, prop, record)
        else:
            record.append(str(v))
            (prop.append(',' + k + ': %s,') 
                if prop[-1][-1] == '}' 
                else prop.append(k + ': %s,'))
    if prop[-1].endswith(','):
        prop[-1] = prop[-1][:-1]
    prop.append('}')

def load_labels_from_file(ag: age.age.Age, 
                          file_path: str, label_name: str) -> None:
    def load_labels_from_csv() -> None:
        header, records = get_contents_from_csv(file_path)
        properties = make_map_csv(header)
        for record in records:
            if label_name == '':
                cursor = ag.execCypher('CREATE (n' + '{' + 
                                       properties + '}) RETURN n',
                                       params=tuple(record))
            else:
                cursor = ag.execCypher('CREATE (n: ' + label_name + '{' + 
                                       properties + '}) RETURN n', 
                                       params=tuple(record))
        ag.commit()

    def load_labels_from_json() -> None:
        with open(file_path, 'r') as openfile:
            json_object = json.load(openfile)
        for i in json_object:
            record, prop = [], ['{']
            make_map_json(i, prop, record)
            prop = ''.join(prop)
            if label_name == '':
                cursor = ag.execCypher('CREATE (n' + prop + ') RETURN n', 
                                       params=tuple(record))
            else:
                cursor = ag.execCypher('CREATE (n: ' + label_name + prop + 
                                       ') RETURN n', 
                                       params=tuple(record))
        ag.commit()


    if file_path.endswith('csv'):
        load_labels_from_csv()
    elif file_path.endswith('json'):
        load_labels_from_json()


def load_edges_from_file(ag: age.age.Age, 
                         file_path: str, label_name: str) -> None:

    def load_edges_from_csv()-> None:
        def get_property_names() -> Tuple[list, list, list]:
            v1_prop_names, v2_prop_names, e_prop_names = [], [], []
            v1_index = header.index('start_vertex_type')
            v2_index = header.index('end_vertex_type')
            for i, property_name in enumerate(header):
                if i < v1_index:
                    v1_prop_names.append(property_name)
                elif (i == v1_index or i == v2_index):
                    continue
                elif (i < v2_index):
                    v2_prop_names.append(property_name)
                else:
                    e_prop_names.append(property_name)
            return v1_prop_names, v2_prop_names, e_prop_names


        def get_property_values() -> Tuple[list, list, list]:
            v1_prop_values, v2_prop_values, e_prop_values = [], [], []
            for i in range(0, num_v1):
                v1_prop_values.append(record[i])
            for i in range(num_v1+1, num_v1+1+num_v2):
                v2_prop_values.append(record[i])
            for i in range(num_v2+1, num_v2+1+num_edge):
                e_prop_values.append(record[i])
            return v1_prop_values, v2_prop_values, e_prop_values


        header, records = get_contents_from_csv(file_path)

        v1_prop_names, v2_prop_names, e_prop_names = get_property_names()
        v1_map, v2_map, e_map = (make_map_csv(v1_prop_names), 
                                make_map_csv(v2_prop_names), 
                                make_map_csv(e_prop_names))
        num_v1, num_v2, num_edge = (len(v1_prop_names), 
                                    len(v2_prop_names), 
                                    len(e_prop_names))
        for record in records:
            (v1_prop_values, 
            v2_prop_values, 
            e_prop_values) = get_property_values()
            v1_label, v2_label = record[num_v1], record[num_v1+num_v2+1]
            cursor = ag.execCypher('MATCH (u: ' + v1_label + '{' + v1_map + 
                                   '}), (v: ' + v2_label  + '{' + v2_map +
                                   '}) CREATE (u)-[e: ' + label_name + 
                                   '{' + e_map + '}' + ']->(v) RETURN e', 
                                params=tuple(v1_prop_values)
                                +tuple(v2_prop_values)
                                +tuple(e_prop_values)) 
        ag.commit()

    def load_edges_from_json()-> None:
        with open(file_path, 'r') as openfile:
            json_object = json.load(openfile)
        for i in json_object:

            v1_prop_vals, v1_prop = [], ['{']
            make_map_json(i['start_vertex'], v1_prop, v1_prop_vals)
            v1_prop = ''.join(v1_prop)
            v1_label = i['start_vertex_type']
            v2_prop_vals, v2_prop = [], ['{']
            make_map_json(i['end_vertex'], v2_prop, v2_prop_vals)
            v2_prop = ''.join(v2_prop)
            v2_label = i['end_vertex_type']
            e_prop_vals, e_prop = [], ['{']
            make_map_json(i['edge'], e_prop, e_prop_vals)
            e_prop = ''.join(e_prop)
            cursor = ag.execCypher('MATCH (u: ' + v1_label + v1_prop + 
                                   '), (v: ' + v2_label + v2_prop + 
                                   ') CREATE (u)-[e: ' + label_name + e_prop + 
                                   ']->(v) RETURN e', 
                                   params = tuple(v1_prop_vals) 
                                   + tuple(v2_prop_vals)
                                   + tuple(e_prop_vals))
        ag.commit()

    if file_path.endswith('csv'):
        load_edges_from_csv()
    elif file_path.endswith('json'):
        load_edges_from_json()