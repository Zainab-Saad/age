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

LOAD 'age';
SET search_path TO ag_catalog;

SELECT create_graph('list_comprehension');

-- Test list comprehensions in the WITH clause
SELECT * FROM cypher('list_comprehension', $$ CREATE(n) WITH n, [FOR u IN [1, 2, 3]] AS u, [FOR v IN [4, 5, 6]] AS v RETURN n, u, v $$) AS (n agtype, u agtype, v agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [FOR u IN [1, 2, 3]] AS u, [FOR i IN [4, 5, 6]] AS v, [FOR u IN [7, 8, 9]] AS w, [FOR u IN [10, 11, 12]] AS x RETURN u, v, w, x $$) AS (u agtype, v agtype, w agtype, x agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS n, [FOR u IN [1, 2, 3]] AS u, [FOR v IN [4, 5, 6]] AS v RETURN u, v, n $$) AS (u agtype, v agtype, n agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [FOR u IN [1, 2, 3]] AS u, [FOR v IN [4, 5, 6]] AS v RETURN u, v, [FOR n IN [7, 8, 9]] $$) AS (u agtype, v agtype, n agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH DISTINCT [FOR u IN [1, 2, 3]] AS u, [FOR v IN [4, 5, 6]] AS v RETURN u, v $$) AS (u agtype, v agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH DISTINCT [FOR u IN [true, true, false] | NOT u] AS u, [FOR v IN [4, 5, 6] | v * 2] AS v RETURN u, v $$) AS (u agtype, v agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [FOR u IN [true, true, false] | NOT u] AS u, [FOR v IN [4, 5, 6] | v * 2]  AS v WHERE u[1] = false RETURN u, v $$) AS (u agtype, v agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [FOR u IN [true, true, false] | NOT u] AS u, [FOR v IN [4, 5, 6] | v * 2]  AS v WHERE u[1] RETURN u, v $$) AS (u agtype, v agtype);

SELECT drop_graph('list_comprehension', true);