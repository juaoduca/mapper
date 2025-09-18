#pragma once

static constexpr const char *SCHEMA_CATALOG_JSON = R"(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/orm-meta-schema.json",
            "$id": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/schema_catalog.json",
            "description": "The catalog Item for a schema - Each item can have many versions",
            "type": "object",
            "name": "schema_catalog",
            "properties": {
                "id": {
                    "type": "integer",
                    "idprop": true,
                    "idkind": "DBSerial"
                },
                "name": {
                    "type": "string",
                    "minLength": 3,
                    "unique": true,
                    "index": true,
                    "required": true,
                },
                "version": {
                    "type": "integer",
                    "descr": "current version applied and in use",
                    "minimum": -1,
                    "default": -1,
                },
                "created_at": {
                    "type": "datetime",
                },
                "updated_at": {
                    "type": "datetime",
                },
            }
        }
    )";

static constexpr const char *SCHEMA_VERSIONS_JSON = R"JSON(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/orm-meta-schema.json",
            "$id": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/schema_versions.json",
            "description": "The versions of a schema item.",
            "type": "object",
            "name": "schema_versions",
            "properties": {
                "id": {
                    "type": "integer",
                    "idprop": true,
                    "idkind": "DBSerial"
                },
                "schema": {
                    "type": "object",
                    "ref_schema": "schema_catalog",
                    "ref_prop": "id",
                    "relation": "one-to-many"
                },
                "version": {
                    "type": "integer",
                    "minimum": 1,
                    "default": 1,
                },
                "applied": {
                    "type": "boolean",
                    "default": false,
                    "descr": "The date and time when this version was applied to db"
                },
                "applied_at": {
                    "type": "datetime"
                },
                "json": {
                    "type": "json",
                }
            },
            "required": ["name", "schema", "version", ],
            "indexes": [
                {"indexName": "idx_schema_version",
                 "fields": ["schema","version"],
                 "unique": true
                 }
            ]
        }
    )JSON";

    static constexpr const char *SCHEMA_USER = R"USER(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/orm-meta-schema.json",
            "$id": "https://raw.githubusercontent.com/juaoduca/public/refs/heads/main/schema_catalog.json",
            "description": "Catalog of versioned JSON Schemas.",
            "type": "object",
            "name": "users",
            "properties": {
                "id": {
                    "type": "integer",
                    "idprop": true,
                    "idkind": "DBSerial"
                },
                "age": {
                    "type": "integer",
                },
                "name": {
                    "type": "string",
                    "unique": true
                }
            },
            "required": ["name", "age"]
        }
    )USER";



