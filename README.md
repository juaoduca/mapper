
Mapper module project

is a module to make Object Relational Mapping

where the objects are defined by a JSON object and NOT by Object Oriented languages objects

The objects are defined using a JSONSchema definition with custom extensions

The objects are stored in DB, loaded at startup, and referenced when needed for CRUD operations

There is a query language inspired by GraphQL to build SQL select script that returns JSON array with objects

the query defined like a GraphQL query, is parsed to an AST that is visited by a vistor who build the select script

the query engine is capable to keep cache of queries and accepts params change to speed up the queries

The mapper create Audit and Track info for each ISERT UPDATE and DELETE operations

The Mapper can send/receive data to/from other server(s) in an Asynch operation, to keep data updated in distributed databases

The mapper work in as a Multi-Tenant mode, with one Database per Tenant, but handling multiple Users / tenants / requests in parallel