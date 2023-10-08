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

#include "access/xact.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/objectaddress.h"
#include "commands/defrem.h"
#include "commands/schemacmds.h"
#include "commands/tablecmds.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/value.h"
#include "parser/parser.h"
#include "utils/fmgroids.h"
#include "utils/relcache.h"
#include "utils/rel.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "commands/graph_commands.h"
#include "commands/label_commands.h"
#include "utils/graphid.h"
#include "utils/load/age_load.h"
#include "utils/load/ag_load_edges.h"
#include "utils/load/ag_load_labels.h"

PG_FUNCTION_INFO_V1(create_complete_graph);

/*
 * SELECT * FROM ag_catalog.create_complete_graph('graph_name',no_of_nodes,
                                            'edge_label', 'node_label'=NULL);
 */

Datum create_complete_graph(PG_FUNCTION_ARGS)
{
    Oid graph_oid;
    Name graph_name;

    int64 no_vertices;
    int64 i,j,vtx_id = 1, edge_id, start_vtx_id, end_vtx_id;

    Name vtx_label_name = NULL;
    Name edge_label_name;
    int32 vtx_label_id;
    int32 edge_label_id;

    agtype *props = NULL;

    char* graph_name_str;
    char* vtx_name_str;
    char* edge_name_str;

    int64 lid;

    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("graph name can not be NULL")));
    }

    if (PG_ARGISNULL(1))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("number of nodes can not be NULL")));
    }

    if (PG_ARGISNULL(2))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("edge label can not be NULL")));
    }

    graph_name = PG_GETARG_NAME(0);
    no_vertices = (int64) PG_GETARG_INT64(1);
    edge_label_name = PG_GETARG_NAME(2);

    graph_name_str = NameStr(*graph_name);
    vtx_name_str = AG_DEFAULT_LABEL_VERTEX;
    edge_name_str = NameStr(*edge_label_name);

    if (!PG_ARGISNULL(3))
    {
        vtx_label_name = PG_GETARG_NAME(3);
        vtx_name_str = NameStr(*vtx_label_name);

        // Check if vertex and edge label are same
        if (strcmp(vtx_name_str, edge_name_str) == 0)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("vertex and edge label can not be same")));
        }
    }

    if (!graph_exists(graph_name_str))
    {
        DirectFunctionCall1(create_graph, CStringGetDatum(graph_name));
    }

    graph_oid = get_graph_oid(graph_name_str);

    if (!PG_ARGISNULL(3))
    {
        // Check if label with the input name already exists
        if (!label_exists(vtx_name_str, graph_oid))
        {
            DirectFunctionCall2(create_vlabel,
                                CStringGetDatum(graph_name),
                                CStringGetDatum(vtx_label_name));
        }
    }

    if (!label_exists(edge_name_str, graph_oid))
    {
        DirectFunctionCall2(create_elabel,
                            CStringGetDatum(graph_name),
                            CStringGetDatum(edge_label_name));
    }

    vtx_label_id = get_label_id(vtx_name_str, graph_oid);
    edge_label_id = get_label_id(edge_name_str, graph_oid);

    props = create_empty_agtype();

    /* Creating vertices*/
    for (i=(int64)1; i<=no_vertices; i++)
    {
        vtx_id = get_new_id(LABEL_KIND_VERTEX, graph_oid);

        insert_vertex_simple(graph_oid, vtx_name_str, vtx_id, props,
                             vtx_label_id);
    }

    lid = vtx_id;

    /* Creating edges*/
    for (i = 1; i<=no_vertices-1; i++)
    {
        start_vtx_id = lid-no_vertices+i;

        for(j=i+1; j<=no_vertices; j++)
        {
            end_vtx_id = lid-no_vertices+j;
            edge_id = get_new_id(LABEL_KIND_EDGE, graph_oid);

            insert_edge_simple(graph_oid, edge_name_str, edge_id,
                               start_vtx_id, end_vtx_id,
                               props, edge_label_id,
                               vtx_label_id, vtx_label_id);
        }
    }

    PG_RETURN_VOID();
}


PG_FUNCTION_INFO_V1(age_create_barbell_graph);

/*
 * The barbell graph is two complete graphs connected by a bridge path
 * Syntax:
 * ag_catalog.age_create_barbell_graph(graph_name Name,
 *                                     m int,
 *                                     n int,
 *                                     vertex_label_name Name DEFAULT = NULL,
 *                                     vertex_properties agtype DEFAULT = NULL,
 *                                     edge_label_name Name DEFAULT = NULL,
 *                                     edge_properties agtype DEFAULT = NULL)
 * Input:
 *
 * graph_name - Name of the graph to be created.
 * m - number of vertices in one complete graph.
 * n - number of vertices in the bridge path.
 * vertex_label_name - Name of the label to assign each vertex to.
 * vertex_properties - Property values to assign each vertex. Default is NULL
 * edge_label_name - Name of the label to assign each edge to.
 * edge_properties - Property values to assign each edge. Default is NULL
 *
 * https://en.wikipedia.org/wiki/Barbell_graph
 */

Datum age_create_barbell_graph(PG_FUNCTION_ARGS)
{
    FunctionCallInfo arguments;
    Oid graph_oid;
    Name graph_name;
    char* graph_name_str;

    int64 start_node_id, end_node_id, edge_id;

    Name node_label_name = NULL;
    int32 node_label_id;
    char* node_label_str;

    Name edge_label_name;
    int32 edge_label_id;
    char* edge_label_str;

    agtype* properties = NULL;

    arguments = fcinfo;

    // Checking for possible NULL arguments
    // Name graph_name
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Graph name cannot be NULL")));
    }

    graph_name = PG_GETARG_NAME(0);
    graph_name_str = NameStr(*graph_name);

    // int graph size (number of nodes in each complete graph)
    if (PG_ARGISNULL(1) && PG_GETARG_INT32(1) < 3)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Graph size cannot be NULL or lower than 3")));
    }

    /*
     * int64 bridge_size: currently only stays at zero.
     * to do: implement bridge with variable number of nodes.
     */
    if (PG_ARGISNULL(2) || PG_GETARG_INT32(2) < 0 )
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Bridge size cannot be NULL or lower than 0")));
    }

    // node label: if null, gets default label, which is "_ag_label_vertex"
    if (PG_ARGISNULL(3))
    {
        namestrcpy(node_label_name, AG_DEFAULT_LABEL_VERTEX);
    }
    else
    {
        node_label_name = PG_GETARG_NAME(3);
    }

    node_label_str = NameStr(*node_label_name);

    /* Name edge_label */
    if (PG_ARGISNULL(5))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("edge label can not be NULL")));
    }

    edge_label_name = PG_GETARG_NAME(5);
    edge_label_str = NameStr(*edge_label_name);


    // create two separate complete graphs
    DirectFunctionCall4(create_complete_graph, arguments->args[0].value,
                                               arguments->args[1].value,
                                               arguments->args[5].value,
                                               arguments->args[3].value);
    DirectFunctionCall4(create_complete_graph, arguments->args[0].value,
                                               arguments->args[1].value,
                                               arguments->args[5].value,
                                               arguments->args[3].value);

    graph_oid = get_graph_oid(graph_name_str);
    node_label_id = get_label_id(node_label_str, graph_oid);
    edge_label_id = get_label_id(edge_label_str, graph_oid);

    // connect a node from each graph
    start_node_id = 1; // first created node, from the first complete graph
    // last created node, second graph
    end_node_id = arguments->args[1].value*2;

    // next index to be assigned to a node or edge
    edge_id = get_new_id(LABEL_KIND_EDGE, graph_oid);

    properties = create_empty_agtype();

    // connect two nodes
    insert_edge_simple(graph_oid, edge_label_str,
                       edge_id, start_node_id,
                       end_node_id, properties,
                       edge_label_id, node_label_id, node_label_id);

    PG_RETURN_VOID();
}