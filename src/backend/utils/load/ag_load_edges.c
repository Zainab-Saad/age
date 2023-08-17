/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "postgres.h"
#include "common/hashfn.h"
#include "utils/snapmgr.h"

#include "utils/load/ag_load_edges.h"
#include "utils/load/age_load.h"
#include "utils/load/csv.h"
#include "utils/agtype.h"


#define VERTEX_HTAB_NAME "Vertex table "
#define VERTEX_HTAB_INITIAL_SIZE 1000000

static vertex_tables_list* list = NULL;

typedef struct vertex_table
{
    int32 label_id;
    char* label_name;
    HTAB* vertex_hashtable;
    vertex_table* next;
} vertex_table;

bool is_vertex_table_loaded(int32 label_id, vertex_tables_list* list)
{
    vertex_table* iterator;
    if (list == NULL)
    {
        return false;
    }
    iterator = list->head;
    while (iterator != NULL)
    {
        if (iterator->label_id == label_id)
        {
            return true;
        }
        iterator = (vertex_table*)iterator->next;
    }
    return false;
}

void append_label_to_list(vertex_table* vertex_tbl, vertex_tables_list** list)
{
    if (*list == NULL)
    {
        *list = (vertex_tables_list*)malloc(sizeof(vertex_tables_list));
        (*list)->head = vertex_tbl;
        (*list)->tail = vertex_tbl;
    }
    else
    {
        (*list)->tail->next = vertex_tbl;
        (*list)->tail = vertex_tbl;
    }
}

vertex_table* get_hashtable_node(int32 label_id, vertex_tables_list* list)
{
    vertex_table* iterator = list->head;
    while (iterator != NULL)
    {
        if (iterator->label_id == label_id)
        {
            return iterator;
        }
        iterator = (vertex_table*)iterator->next;
    }
    return NULL;
}

void load_vertex_table(char* label_name, int32 label_id, Oid graph_oid,
                       char* graph_name, vertex_tables_list** list)
{

    HASHCTL vertex_ctl;
    int vlen;
    int glen;
    char* vhn;
    Relation graph_vertex_label;
    TableScanDesc scan_desc;
    HeapTuple tuple;
    Oid vertex_label_table_oid;
    TupleDesc tupdesc;
    Snapshot snapshot;
    vertex_table* vertex_tbl = (vertex_table*)malloc(sizeof(vertex_table));
    vertex_tbl->label_id = label_id;
    vertex_tbl->label_name = label_name;
    vertex_tbl->next = NULL;

    // get the len of the vertex hashtable and graph name
    vlen = strlen(VERTEX_HTAB_NAME);
    glen = strlen(graph_name);
    // allocate the space for the vertex hashtable name
    vhn = palloc0(vlen + glen + 1);
    // copy the name
    strcpy(vhn, VERTEX_HTAB_NAME);
    /* add in the graph name */
    vhn = strncat(vhn, graph_name, glen);

    /* initialize the vertex hashtable */
    MemSet(&vertex_ctl, 0, sizeof(vertex_ctl));
    vertex_ctl.keysize = sizeof(int64);
    vertex_ctl.entrysize = sizeof(vertex_table);
    vertex_ctl.hash = tag_hash;

    vertex_tbl->vertex_hashtable = hash_create(vhn, VERTEX_HTAB_INITIAL_SIZE,
                                          &vertex_ctl,
                                          HASH_ELEM | HASH_FUNCTION);

    pfree(vhn);

    snapshot = GetActiveSnapshot();

    /* get the vertex label name's OID */
    vertex_label_table_oid = get_relname_relid(label_name,
                                                graph_oid);
    /* open the relation (table) and begin the scan */
    graph_vertex_label = table_open(vertex_label_table_oid, ShareLock);
    scan_desc = table_beginscan(graph_vertex_label, snapshot, 0, NULL);
    /* get the tupdesc - we don't need to release this one */
    tupdesc = RelationGetDescr(graph_vertex_label);
    /* bail if the number of columns differs */
    if (tupdesc->natts != Natts_ag_label_vertex)
    {
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                    errmsg("Invalid number of attributes for %s.%s",
                    graph_name, label_name)));
    }

    /* get all tuples in table and insert them into graph hashtables */
    while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
    {
        int64 vertex_id;
        Datum vertex_properties;

        agtype* vertex_properties_agt;
        // csv reads the properties as string
        agtype_value* result_str;
        // store the id property as int instead of string
        int64 result;

        vertex_entry *ve = NULL;
        bool found = false;

        /* something is wrong if this isn't true */
        Assert(HeapTupleIsValid(tuple));
        /* get the vertex id */
        vertex_id = DatumGetInt64(column_get_datum(tupdesc, tuple, 0, "id",
                                                    INT8OID, true));
        /* get the vertex properties datum */
        vertex_properties = column_get_datum(tupdesc, tuple, 1,
                                                "properties", AGTYPEOID, true);

        vertex_properties_agt = DATUM_GET_AGTYPE_P(vertex_properties);

        result_str = execute_map_access_operator_internal
                    (vertex_properties_agt, NULL, "__id__", 6);
        result = result_str->val.int_value;


        /* search for the vertex */
        ve = (vertex_entry *)hash_search(vertex_tbl->vertex_hashtable,
                                            (void *)&result, HASH_ENTER,
                                             &found);

        /* this insert must not fail, it means there is a duplicate */
        if (found)
        {
            elog(ERROR, "failed to build hashtable for label %s"
                        " due to duplicate id property",
                        label_name);
        }

        /* again, MemSet may not be needed here */
        MemSet(ve, 0, sizeof(vertex_entry));

        ve->property_id = result;
        ve->vertex_id = vertex_id;
    }

    /* end the scan and close the relation */
    table_endscan(scan_desc);
    table_close(graph_vertex_label, ShareLock);

    append_label_to_list(vertex_tbl, list);
}

void edge_field_cb(void *field, size_t field_len, void *data)
{

    csv_edge_reader *cr = (csv_edge_reader*)data;
    if (cr->error)
    {
        cr->error = 1;
        ereport(NOTICE,(errmsg("There is some unknown error")));
    }

    // check for space to store this field
    if (cr->cur_field == cr->alloc)
    {
        cr->alloc *= 2;
        cr->fields = realloc(cr->fields, sizeof(char *) * cr->alloc);
        cr->fields_len = realloc(cr->header, sizeof(size_t *) * cr->alloc);
        if (cr->fields == NULL)
        {
            cr->error = 1;
            ereport(ERROR,
                    (errmsg("field_cb: failed to reallocate %zu bytes\n",
                            sizeof(char *) * cr->alloc)));
        }
    }
    cr->fields_len[cr->cur_field] = field_len;
    cr->curr_row_length += field_len;
    cr->fields[cr->cur_field] = strndup((char*)field, field_len);
    cr->cur_field += 1;
}

// Parser calls this function when it detects end of a row
void edge_row_cb(int delim __attribute__((unused)), void *data)
{
    csv_edge_reader *cr = (csv_edge_reader*)data;

    size_t i, n_fields;
    int64 start_id_int;
    int64 start_vertex_id;
    int start_vertex_type_id;

    int64 end_id_int;
    int64 end_vertex_id;
    int end_vertex_type_id;

    agtype* props = NULL;

    bool found_v1 = false;
    bool found_v2 = false;

    vertex_entry *ve_1 = NULL;
    vertex_entry *ve_2 = NULL;

    vertex_table* v1_hashtable_node =  NULL;
    vertex_table* v2_hashtable_node = NULL;

    n_fields = cr->cur_field;


    if (cr->row == 0)
    {
        cr->header_num = cr->cur_field;
        cr->header_row_length = cr->curr_row_length;
        cr->header_len = (size_t* )malloc(sizeof(size_t *) * cr->cur_field);
        cr->header = malloc((sizeof (char*) * cr->cur_field));

        for (i = 0; i<cr->cur_field; i++)
        {
            cr->header_len[i] = cr->fields_len[i];
            cr->header[i] = strndup(cr->fields[i], cr->header_len[i]);
        }
    }
    else
    {
        int64 new_id = get_new_id(LABEL_KIND_EDGE, cr->graph_oid);

        start_id_int = strtol(cr->fields[0], NULL, 10);
        start_vertex_type_id = get_label_id(cr->fields[1], cr->graph_oid);
        end_id_int = strtol(cr->fields[2], NULL, 10);
        end_vertex_type_id = get_label_id(cr->fields[3], cr->graph_oid);
        /*
         * if start vertex label table is not in memory
         * i.e; (we have not made its hashtable yet)
         */
        if (!is_vertex_table_loaded(start_vertex_type_id, list))
        {
            load_vertex_table(cr->fields[1], start_vertex_type_id,
                                cr->graph_oid, cr->graph_name, &list);
        }

        /*
         * if end vertex label table is not in memory
         * i.e; (we have not made its hashtable yet)
         */
        if (!is_vertex_table_loaded(end_vertex_type_id, list))
        {
            load_vertex_table(cr->fields[3], end_vertex_type_id,
                                cr->graph_oid, cr->graph_name, &list);
        }

        // get the start and end vertex table's hashtable
        v1_hashtable_node = get_hashtable_node(start_vertex_type_id, list);
        v2_hashtable_node = get_hashtable_node(end_vertex_type_id, list);

        /* search for the start vertex in the hashtable */
        ve_1 = (vertex_entry *)hash_search(v1_hashtable_node->vertex_hashtable,
                                            (void *)&start_id_int, HASH_ENTER,
                                            &found_v1);
        if (!found_v1)
        {
            elog(ERROR, "start vertex not found in hashtable");
        }
        /* search for the end vertex in the hashtable */
        ve_2 = (vertex_entry *)hash_search(v2_hashtable_node->vertex_hashtable,
                                            (void *)&end_id_int, HASH_ENTER,
                                            &found_v2);
        if (!found_v2)
        {
            elog(ERROR, "end vertex not found in hashtable");
        }

        // set the start and end vertex ids
        start_vertex_id = ve_1->vertex_id;
        end_vertex_id = ve_2->vertex_id;

        props = create_agtype_from_list_i(cr->header, cr->fields,
                                          n_fields, 3);

        insert_edge_simple(cr->graph_oid, cr->object_name,
                           new_id, start_vertex_id,
                           end_vertex_id, props, cr->object_id);
        pfree(props);
    }

    for (i = 0; i < n_fields; ++i)
    {
        free(cr->fields[i]);
    }

    if (cr->error)
    {
        ereport(NOTICE,(errmsg("There is some error")));
    }


    cr->cur_field = 0;
    cr->curr_row_length = 0;
    cr->row += 1;
}

static int is_space(unsigned char c)
{
    if (c == CSV_SPACE || c == CSV_TAB)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int is_term(unsigned char c)
{
    if (c == CSV_CR || c == CSV_LF)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int create_edges_from_csv_file(char *file_path,
                               char *graph_name,
                               Oid graph_oid,
                               char *object_name,
                               int object_id )
{

    FILE *fp;
    struct csv_parser p;
    char buf[1024];
    size_t bytes_read;
    unsigned char options = 0;
    csv_edge_reader cr;

    if (csv_init(&p, options) != 0)
    {
        ereport(ERROR,
                (errmsg("Failed to initialize csv parser\n")));
    }

    csv_set_space_func(&p, is_space);
    csv_set_term_func(&p, is_term);

    fp = fopen(file_path, "rb");
    if (!fp)
    {
        ereport(ERROR,
                (errmsg("Failed to open %s\n", file_path)));
    }


    memset((void*)&cr, 0, sizeof(csv_edge_reader));
    cr.alloc = 128;
    cr.fields = malloc(sizeof(char *) * cr.alloc);
    cr.fields_len = malloc(sizeof(size_t *) * cr.alloc);
    cr.header_row_length = 0;
    cr.curr_row_length = 0;
    cr.graph_name = graph_name;
    cr.graph_oid = graph_oid;
    cr.object_name = object_name;
    cr.object_id = object_id;

    while ((bytes_read=fread(buf, 1, 1024, fp)) > 0)
    {
        if (csv_parse(&p, buf, bytes_read, edge_field_cb,
                      edge_row_cb, &cr) != bytes_read)
        {
            ereport(ERROR, (errmsg("Error while parsing file: %s\n",
                                   csv_strerror(csv_error(&p)))));
        }
    }

    csv_fini(&p, edge_field_cb, edge_row_cb, &cr);

    if (ferror(fp))
    {
        ereport(ERROR, (errmsg("Error while reading file %s\n", file_path)));
    }

    fclose(fp);

    free(cr.fields);
    // deallocate the memory allocated to vertex_tables
    if (list != NULL)
    {
        vertex_table* iterator_next = NULL;
        vertex_table* iterator = list->head;

        while(iterator != NULL)
        {
            iterator_next = iterator->next;
            free(iterator);
            iterator = iterator_next;
        }
        list = NULL;
        free(list);
    }
    csv_free(&p);
    return EXIT_SUCCESS;
}
