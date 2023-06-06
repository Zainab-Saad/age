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

def load_labels_into_file(ag: age.age.Age, dir_path: str) -> None:
    
    def load_labels_into_csv():
        file_path = dir_path + row[0] + '.csv'
        with open(file_path, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(list((f_vertices[0][0].properties).keys()))
            for each_vertex in f_vertices:
                writer.writerow(list((each_vertex[0].properties).values()))
        file.close()
        
    def load_labels_into_json():
        file_path = dir_path + row[0] + '.json'
        json_list = [vertex[0].properties for vertex in f_vertices]
        json_object = json.dumps(json_list, indent=4)
        with open(file_path, mode='w') as file:
            file.write(json_object)
        file.close()
        
    # TODO -- this is surely an overhead
    cursor = ag.execCypher('MATCH(u) RETURN DISTINCT labels(u)[0]')
    for row in cursor:
        vertices = ag.execCypher('MATCH (u:' + row[0] + ') RETURN u')
        f_vertices = vertices.fetchall()
        var = any(isinstance(i,dict) for j in f_vertices 
                  for i in j[0].properties.values())
        load_labels_into_json() if var else load_labels_into_csv()
    
def load_edges_into_file(ag: age.age.Age, dir_path: str) -> None:
    
    cursor = ag.execCypher('MATCH (u)-[e]->(v) RETURN DISTINCT type(e)')
    
    def load_edges_into_csv():
        file_path = dir_path + row[0] + '.csv'
        with open(file_path, mode='w', newline='') as file:
            writer = csv.writer(file)
            header = (list(f_edges[0][0][0].properties.keys()) +
                      ['start_vertex_type'] +
                      list(f_edges[0][0][2].properties.keys()) +
                      ['end_vertex_type'] +
                      list(f_edges[0][0][1].properties.keys()))
            writer.writerow(header)
            for each_edge in f_edges:
                writer = csv.writer(file)
                record =  (list(each_edge[0][0].properties.values()) + 
                           [each_edge[0][0].label, ] + 
                           list(each_edge[0][2].properties.values()) + 
                           [each_edge[0][2].label, ] + 
                           list(each_edge[0][1].properties.values()))
                writer.writerow(record)
        file.close()
    def load_edges_into_json():
        file_path = dir_path + row[0] + '.json'
        json_list = []
        for path in f_edges:
            dict1 = {}
            dict1['start_vertex'] = path[0][0].properties
            dict1['start_vertex_type'] = path[0][0].label
            dict1['end_vertex'] = path[0][2].properties
            dict1['end_vertex_type'] = path[0][2].label
            dict1['edge'] = path[0][1].properties
            # dict1.update(path[0][1].properties)
            json_list.append(dict1)
        json_object = json.dumps(json_list, indent=4)
        with open(file_path, mode='w') as file:
            file.write(json_object)
        file.close()
            
    for row in cursor:
        edges = ag.execCypher('MATCH p = (u)-[e:' + row[0] + 
                              ']->(v) RETURN p')
        f_edges = edges.fetchall()
        var = (any(isinstance(i,dict) for j in f_edges 
                  for i in j[0][1].properties.values()) or 
               any(isinstance(i,dict) for j in f_edges 
                  for i in j[0][0].properties.values()) or 
               any(isinstance(i,dict) for j in f_edges 
                  for i in j[0][2].properties.values()))
        load_edges_into_json() if var else load_edges_into_csv()
        
