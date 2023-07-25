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

#include "math.h"

#include "postgres.h"

#include "access/heapam.h"
#include "utils/snapmgr.h"

#include "utils/load/ag_load_edges.h"
#include "utils/load/age_load.h"
#include "utils/load/csv.h"
#include "utils/agtype.h"

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
    int64 start_vertex_graph_id = 0;

    int64 end_id_int;
    int64 end_vertex_graph_id = 0;

    int label_id;

    Snapshot snapshot;
    TableScanDesc scan_desc;
    HeapTuple tuple;
    TupleDesc tupdesc;

    Relation start_label_relation;
    Oid start_label_table_oid;

    Relation end_label_relaion;
    Oid end_label_table_oid;

    agtype* props = NULL;

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
        end_id_int = strtol(cr->fields[2], NULL, 10);

        snapshot = GetActiveSnapshot();

        start_label_table_oid = get_relname_relid(cr->fields[1], cr->graph_oid);
        start_label_relation = table_open(start_label_table_oid, ShareLock);
        scan_desc = table_beginscan(start_label_relation, snapshot, 0, NULL);
        tupdesc = RelationGetDescr(start_label_relation);

        if (tupdesc->natts != Natts_ag_label_vertex)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_TABLE),
                     errmsg("Invalid number of attributes for %s.%s",
                     cr->graph_name, cr->fields[1])));
        }

        while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
        {
            graphid vertex_id;
            Datum vertex_properties;
            agtype* vertex_properties_agt;
            // agtype_value* vertex_properties_agtvp;
            agtype_value* result;

            /* something is wrong if this isn't true */
            Assert(HeapTupleIsValid(tuple));
            /* get the vertex id */
            vertex_id = DatumGetInt64(column_get_datum(tupdesc, tuple, 0, "id",
                                                       INT8OID, true));
            /* get the vertex properties datum */
            vertex_properties = column_get_datum(tupdesc, tuple, 1,
                                                 "properties", AGTYPEOID, true);

            vertex_properties_agt = DATUM_GET_AGTYPE_P(vertex_properties);

            result = execute_map_access_operator_internal
                        (vertex_properties_agt, NULL, "id", 2);

            if (strtol(result->val.string.val, NULL, 10) == start_id_int) 
            {
                start_vertex_graph_id = vertex_id;
                break;
            }
        }

        /* end the scan and close the relation */
        table_endscan(scan_desc);
        table_close(start_label_relation, ShareLock);

        end_label_table_oid = get_relname_relid(cr->fields[3], cr->graph_oid);
        end_label_relaion = table_open(end_label_table_oid, ShareLock);
        scan_desc = table_beginscan(end_label_relaion, snapshot, 0, NULL);
        tupdesc = RelationGetDescr(end_label_relaion);

        if (tupdesc->natts != Natts_ag_label_vertex)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_TABLE),
                     errmsg("Invalid number of attributes for %s.%s",
                     cr->graph_name, cr->fields[3])));
        }

        while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
        {
            graphid vertex_id;
            Datum vertex_properties;
            agtype* vertex_properties_agt;
            // agtype_value* vertex_properties_agtvp;
            agtype_value* result;

            /* something is wrong if this isn't true */
            Assert(HeapTupleIsValid(tuple));
            /* get the vertex id */
            vertex_id = DatumGetInt64(column_get_datum(tupdesc, tuple, 0, "id",
                                                       INT8OID, true));
            /* get the vertex properties datum */
            vertex_properties = column_get_datum(tupdesc, tuple, 1,
                                                 "properties", AGTYPEOID, true);

            vertex_properties_agt = DATUM_GET_AGTYPE_P(vertex_properties);

            result = execute_map_access_operator_internal(vertex_properties_agt, NULL, "id", 2);

            if (strtol(result->val.string.val, NULL, 10) == end_id_int) 
            {
                end_vertex_graph_id = vertex_id;
                break;
            }
        }

        /* end the scan and close the relation */
        table_endscan(scan_desc);
        table_close(end_label_relaion, ShareLock);

        if (start_vertex_graph_id != 0 && end_vertex_graph_id != 0) {
            props = create_agtype_from_list_i(cr->header, cr->fields,
                                            n_fields, 3);
            label_id = get_label_id(cr->object_name, cr->graph_oid);
            insert_edge_simple(cr->graph_oid, cr->object_name,
                            new_id, start_vertex_graph_id,
                            end_vertex_graph_id, props, label_id);
        }
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
    csv_free(&p);
    return EXIT_SUCCESS;
}
