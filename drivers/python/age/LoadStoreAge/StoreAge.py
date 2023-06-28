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

import age, csv, json, os


def load_labels_into_file(conn, GRAPH_NAME, dir_path: str) -> None:
    def load_labels_into_csv():
        # storing the data into csv and meta data into json
        # storing data
        data_file = each_vertex_label[0] + '.csv'
        with open(os.path.join(data_dir_path, data_file), 'w') as file:
            writer = csv.writer(file)
            writer.writerow(['graphid']+ list((vertices[0][1]).keys()))
            for each_vertex in vertices:
                writer.writerow([each_vertex[0]] + list((each_vertex[1]).values()))
        file.close()
        # storing metadata
        stmt = '''SELECT id FROM ag_catalog.ag_label WHERE relation = '%s."%s"'::regclass;''' % (GRAPH_NAME, each_vertex_label[0])

        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                rows = cursor.fetchall()
            except Exception as ex:
                pass
                conn.rollback()
        label_id = rows[0][0]

        # store this label_id in json
        meta_data_file = 'meta_' + each_vertex_label[0] + '.json'
        json_object = json.dumps({
            "label_id" : label_id,
        }, indent=4)
        with open(os.path.join(meta_data_dir_path, meta_data_file), 'w') as file:
            file.write(json_object)
        file.close()


 
    def load_labels_into_json():
        json_list = []
        for each_vertex in vertices:
            json_dict = {}
            json_dict['graphid'] = each_vertex[0]
            json_dict['properties'] = each_vertex[1]
            json_list.append(json_dict)
        json_object = json.dumps(json_list, indent=4)
        data_file = each_vertex_label[0] + '.json'
        with open(os.path.join(data_dir_path, data_file), 'w') as file:
            file.write(json_object)
        
        file.close()

        # storing metadata
        stmt = '''SELECT id FROM ag_catalog.ag_label WHERE relation = '%s."%s"'::regclass;''' % (GRAPH_NAME, each_vertex_label[0])
        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                rows = cursor.fetchall()
            except Exception as ex:
                pass
                conn.rollback()
        label_id = rows[0][0]

        # store this label_id in json
        meta_data_file = 'meta_' + each_vertex_label[0] + '.json'
        json_object = json.dumps({
            "label_id" : label_id,
        }, indent=4)
        with open(os.path.join(meta_data_dir_path, meta_data_file), 'w') as file:
            file.write(json_object)
        file.close()

    age.setUpAge(conn, GRAPH_NAME)
    # path construction
    data_dir = 'data'
    meta_data_dir = 'meta_data'
    # creating directories for storing data and metadata
    try:
        data_dir_path = os.path.join(dir_path, data_dir)
        os.mkdir(data_dir_path)
    except Exception as ex:
        print(type(ex), ex)

    try:
        meta_data_dir_path = os.path.join(dir_path, meta_data_dir)
        os.mkdir(meta_data_dir_path)
    except Exception as ex:
        print(type(ex), ex)


    with conn.cursor() as cursor:
        try:
            cursor.execute('''
                SELECT c.relname                                                                                       
                FROM pg_inherits 
                JOIN pg_class AS c ON (inhrelid=c.oid)
                JOIN pg_class as p ON (inhparent=p.oid)
                JOIN pg_namespace pn ON pn.oid = p.relnamespace
                JOIN pg_namespace cn ON cn.oid = c.relnamespace
                WHERE p.relname = '_ag_label_vertex' and pn.nspname = %s;  
            ''', (GRAPH_NAME,))
            vertex_labels = cursor.fetchall()
        except Exception as ex: 
            conn.rollback()


    for each_vertex_label in vertex_labels:
        # TODO
        stmt = 'SELECT id, properties FROM %s."%s"' % (GRAPH_NAME, each_vertex_label[0])
        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                vertices = cursor.fetchall()
            except Exception as ex:
                conn.rollback()
        var = any(isinstance(i,dict) for j in vertices 
                  for i in j[1].values())
        load_labels_into_json() if var else load_labels_into_csv()

def load_edges_into_file(conn, GRAPH_NAME, dir_path: str) -> None:

    def load_edges_into_csv():
        # storing the data into csv and meta data into json
        # storing data
        data_file = each_edge_label[0] + '.csv'
        with open(os.path.join(data_dir_path, data_file), 'w') as file:
            writer = csv.writer(file)
            writer.writerow(['graphid']+ ['start_graphid'] + ['end_graphid'] + list((edges[0][3]).keys()))
            for each_edge in edges:
                writer.writerow([each_edge[0]] + [each_edge[1]] + [each_edge[2]] + list((each_edge[3]).values()))
        file.close()


        # storing metadata
        stmt = '''SELECT id FROM ag_catalog.ag_label WHERE relation = '%s."%s"'::regclass;''' % (GRAPH_NAME, each_edge_label[0])

        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                rows = cursor.fetchall()
            except Exception as ex:
                pass
                conn.rollback()
        label_id = rows[0][0]

        # store this label_id in json
        meta_data_file = 'meta_' + each_edge_label[0] + '.json'
        json_object = json.dumps({
            "label_id" : label_id,
        }, indent=4)
        with open(os.path.join(meta_data_dir_path, meta_data_file), 'w') as file:
            file.write(json_object)
        file.close()




    def load_edges_into_json():
        # storing the data and metadata into json
        # storing data
        json_list = []
        for each_edge in edges:
            json_dict = {}
            json_dict['graphid'] = each_edge[0]
            json_dict['start_graphid'] = each_edge[1]
            json_dict['end_graphid'] = each_edge[2]
            json_dict['properties'] = each_edge[3]
            json_list.append(json_dict)
        json_object = json.dumps(json_list, indent=4)
        data_file = each_edge_label[0] + '.json'
        with open(os.path.join(data_dir_path, data_file), 'w') as file:
            file.write(json_object)
        
        file.close()

        # storing metadata
        stmt = '''SELECT id FROM ag_catalog.ag_label WHERE relation = '%s."%s"'::regclass;''' % (GRAPH_NAME, each_edge_label[0])
        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                rows = cursor.fetchall()
            except Exception as ex:
                pass
                conn.rollback()
        label_id = rows[0][0]

        # store this label_id in json
        meta_data_file = 'meta_' + each_edge_label[0] + '.json'
        json_object = json.dumps({
            "label_id" : label_id,
        }, indent=4)
        with open(os.path.join(meta_data_dir_path, meta_data_file), 'w') as file:
            file.write(json_object)
        file.close()


    age.setUpAge(conn, GRAPH_NAME)
    # path construction
    data_dir = 'data'
    meta_data_dir = 'meta_data'
    # creating directories for storing data and metadata
    try:
        data_dir_path = os.path.join(dir_path, data_dir)
        os.mkdir(data_dir_path)
    except Exception as ex:
        pass

    try:
        meta_data_dir_path = os.path.join(dir_path, meta_data_dir)
        os.mkdir(meta_data_dir_path)
    except Exception as ex:
        pass

    with conn.cursor() as cursor:
        try:
            cursor.execute('''
                SELECT c.relname                                                                                       
                FROM pg_inherits 
                JOIN pg_class AS c ON (inhrelid=c.oid)
                JOIN pg_class as p ON (inhparent=p.oid)
                JOIN pg_namespace pn ON pn.oid = p.relnamespace
                JOIN pg_namespace cn ON cn.oid = c.relnamespace
                WHERE p.relname = '_ag_label_edge' and pn.nspname = %s;  
            ''', (GRAPH_NAME,))
            edge_labels = cursor.fetchall()
        except Exception as ex: 
            conn.rollback()

    for each_edge_label in edge_labels:
        # TODO
        stmt = 'SELECT * FROM %s."%s"' % (GRAPH_NAME, each_edge_label[0])
        with conn.cursor() as cursor:
            try:
                cursor.execute(stmt)
                edges = cursor.fetchall()
            except Exception as ex:
                conn.rollback()
        var = any(isinstance(i,dict) for j in edges 
                  for i in j[3].values())
        load_edges_into_json() if var else load_edges_into_csv()