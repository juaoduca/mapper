#pragma once

static constexpr const char *SCHEMA_CATALOG_JSON = R"JSON(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/mapper/refs/heads/main/orm-meta-schema.json",
            "$id": "https://example.com/schemas/schema_catalog.json",
            "description": "Catalog of versioned JSON Schemas.",
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
                "index": true
                },
                "version": {
                "type": "integer",
                "minimum": 0,
                "default": "0"
                },
                "created_at": {
                "type": "datetime",
                "default": "datetime('now')"
                },
                "updated_at": {
                "type": "datetime",
                "default": "datetime('now')"
                }
            },
            "required": ["name", "version", "created_at", "updated_at"]
        }
    )JSON";

static constexpr const char *SCHEMA_VERSIONS_JSON = R"JSON(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/mapper/refs/heads/main/orm-meta-schema.json",
            "$id": "https://example.com/schemas/schema_catalog.json",
            "description": "Catalog of versioned JSON Schemas.",
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
                "schema": "schema_catalog",
                "prop": "id",
                "relation": "one-to-many"
                },
                "version": {
                "type": "integer",
                "minimum": 1,
                "default": "1"
                "unique": true
                },
                "applied": {
                "type": "boolean",
                "default": false
                },
                "json": {
                "type": "json",
                "default": ""
                }
            },
            "required": ["name", "schema", "version"]
        }
    )JSON";

    static constexpr const char *SCHEMA_USERS = R"(
        {
            "$schema": "https://raw.githubusercontent.com/juaoduca/mapper/refs/heads/main/orm-meta-schema.json",
            "$id": "https://example.com/schemas/schema_catalog.json",
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
    )";



