"""
Author: Zainab Saad
GitHub: Zainab-Saad
"""
import age, csv, copy
from typing import Tuple

# Load CSV to AGE 
def get_contents_from_csv(file_path) -> Tuple[list, list]:
    """
    Parameters:
    file: path of the csv file

    Returns:
    header[], records [[]]
    """
    # open file 
    file = open(file_path)
    # use csv.reader object to read the csv file
    csvreader = csv.reader(file)
    # extract field names
    header = []
    header = next(csvreader)
    # extract the records
    records = []
    for record in csvreader:
        records.append(record)
    return header, records

def make_map(header: str) -> str:
    """
    Parameters:
    header: names of properties of vertex/edge

    Returns:
    string --> property_name: property_value(%s)
    """
    properties = ''
    for each_property in header:
        properties += each_property + ': %s, '
    return properties[0: len(properties)-2]

def load_labels_from_file(ag: age.age.Age, csv_file_path: str, label_name: str) -> None:
    """"
    Parameters:
    connection object: connection to postgresql using age extension
    file path
    label name
    """
    
    # get property names, property values from csv
    header, records = get_contents_from_csv(csv_file_path)
    # get the properties string to make the vertex/node
    properties = make_map(header)
    for record in records:
        if label_name == '':
            cursor = ag.execCypher('CREATE (n' + '{' + properties + '}) RETURN n', params=tuple(record))
        else:
            cursor = ag.execCypher('CREATE (n: ' + label_name + '{' + properties + '}) RETURN n', params=tuple(record))
        for row in cursor:
            print('CREATED : ', row)
            # explicitly commit to save changes
            ag.commit()

def load_edges_from_file(ag: age.age.Age, csv_file_path: str, label_name: str) -> None:

    # define an inner function to get the names of all the properties that will be used for matching vertices (in case more than one property is given)
    def get_property_names() -> Tuple[list, list]:
        first_vertex_property_names, second_vertex_property_names = [], []
        count = 0
        iterator = header[count]
        while (iterator != 'start_vertex_type'):
            first_vertex_property_names.append(iterator)
            count += 1
            iterator = header[count]
        count += 1
        iterator = header[count]
        while (iterator != 'end_vertex_type'):
            second_vertex_property_names.append(iterator)
            count += 1
            iterator = header[count]
        return first_vertex_property_names, second_vertex_property_names
    
    # inner function to get the values of all properties for both vertices
    def get_property_values():
        first_vertex_property_values, second_vertex_property_values = [], []
        for i in range(0, num_first_vertex):
            first_vertex_property_values.append(record[i])
        for i in range(num_first_vertex+1, num_first_vertex+1+num_second_vertex):
            second_vertex_property_values.append(record[i])
        return first_vertex_property_values, second_vertex_property_values

    # get all the edges
    header, records = get_contents_from_csv(csv_file_path)
    # get all the properties for matching each vertex
    first_vertex_property_names, second_vertex_property_names = get_property_names()
    for record in records:
        num_first_vertex, num_second_vertex = len(first_vertex_property_names), len(second_vertex_property_names)
        first_vertex_property_values, second_vertex_property_values = get_property_values()
        first_vertex_label, second_vertex_label = record[num_first_vertex], record[num_first_vertex+num_second_vertex+1]
        properties_map_1, properties_map_2 = make_map(first_vertex_property_names), make_map(second_vertex_property_names)
        cursor = ag.execCypher('MATCH (u: ' + first_vertex_label + '{' + properties_map_1 + '}), (v: ' + second_vertex_label  + '{' + properties_map_2 +'}) CREATE (u)-[e: ' + label_name + ']->(v) RETURN e', 
                               params=tuple(first_vertex_property_values)+tuple(second_vertex_property_values))
        for row in cursor:
            print('CREATED: ', row)
            # commit explicitly to save the changes
            ag.commit()
            
            
# Load AGE to CSV
def load_labels_into_file(ag: age.age.Age,  graph_name: str) -> None:
    """
    Paramters:
    Age class object
    graph name
    """

    cursor = ag.execCypher('MATCH (u) RETURN u')
    # create a seperate csv file for each label and write the properties header, records there
    cursor_fetch = cursor.fetchall()
    label , file_path = '', ''
    for i,row in enumerate(cursor_fetch):
        file_path = './age_load/data/' + label + '.csv'
        row_label = list(cursor_fetch)[i][0].label
        if(row_label != label):
            label = row_label
            file_path = './age_load/data/' + label + '.csv'
            with open(file_path, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow(list((row[0].properties).keys()))
                writer.writerow(list((row[0].properties).values()))
        else:
            with open(file_path, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow(list((row[0].properties).values()))

    
def load_edges_into_file(ag: age.age.Age, graph_name: str) -> None:
    cursor = ag.execCypher('MATCH p = ()-[]->() RETURN p')
    # create a seperate csv file for each label and write the properties header, records there
    cursor_fetch = cursor.fetchall()
    label , file_path = '', ''
    for i,row in enumerate(cursor_fetch):
        file_path = './age_load/data/' + label + '.csv'
        row_label = row[0][1].label
        header = list(row[0][0].properties.keys()) + ['start_vertex_type'] + list(row[0][2].properties.keys()) + ['end_vertex_type']
        record =  list(row[0][0].properties.values()) + [row[0][0].label, ] + list(row[0][2].properties.values()) + [row[0][2].label, ] 
        print(header)
        print(record)
        if(row_label != label):
            label = row_label
            file_path = './age_load/data/' + label + '.csv'
            with open(file_path, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow(header)
                writer.writerow(record)
        else:
            with open(file_path, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow(record) 
