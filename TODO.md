Done :

Schema definition

Schema Parsing into a OrmSchema / OrmProp C++ Objects

Schema Storage in a DB catalog with version control

Schema Bootstrap at first startup to init the schema control and storage

Schema catalog add / remove / retrive of schemas

Schema Mapping to DB Table(s) with ORM Hierarchy strategy

Schema Mapping update to new version of Schema definition

GraphQL like query language support

Schema Query Language based on GraphQL,
    With an AST parser and Visitor producing a
    SELECT script returning a JSON Array of JSON Objects
    Alias Field Func Agg in the form alias:field.agg || alias:field.func.agg
    Func can be agg func avg, sum
    OR date time transform strings
    Datime time can be grouped too
    Ref object to FK JOIN {dog {id, name, owner {id name phone}}}

TODO :

Schema Loading of stored Schema to an in-memory Catalog

Schema Query Language based on GraphQL, With an AST parser and Visitor producing a SELECT script returning a JSON Array of JSON Objects
    Nested power-ups: allow owner { createdAt.date.groupby, id.count } (scoped agg/group-by inside FK blocks).

    Filtering on nested fields: { Dog(where:{ owner.name:"Bob" }) { id, owner { name } } } → push predicate into the FK LEFT JOIN or WHERE.

    Multiple FKs & aliasing: prove collisions don’t happen with two FK OBJECT props to the same table (e.g., owner, vet).

    Depth & cycles: cap recursion depth; detect cycles in schemas to avoid infinite JOINs.

    Ordering & paging: support orderBy keys (including derived dt keys) and limit/offset with stable deterministic order.

    Hardening tests:

    null FK → nested owner should be null (JSON) not {}.

    negative: nested .groupby or aggregates should throw (you already guard this).



Audit and Track info by user / tenant / origim datetime and manipulated data

Data syncronization with distributed servers

Internal Data notification for data monitoring agents

Pub-Sub service, with custom data monitoring agents pushing subscribers notifications.