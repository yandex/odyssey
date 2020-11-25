#ifndef KIWI_ERROR_CODES_H
#define KIWI_ERROR_CODES_H

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

/* Based on PostgreSQL 9.6 error codes src/backend/errorcodes.txt */

/* Class 00 - Successful Completion */
#define KIWI_SUCCESSFUL_COMPLETION "00000"

/* Class 01 - Warning */
#define KIWI_WARNING                                       "01000"
#define KIWI_WARNING_DYNAMIC_RESULT_SETS_RETURNED          "0100C"
#define KIWI_WARNING_IMPLICIT_ZERO_BIT_PADDING             "01008"
#define KIWI_WARNING_NULL_VALUE_ELIMINATED_IN_SET_FUNCTION "01003"
#define KIWI_WARNING_PRIVILEGE_NOT_GRANTED                 "01007"
#define KIWI_WARNING_PRIVILEGE_NOT_REVOKED                 "01006"
#define KIWI_WARNING_STRING_DATA_RIGHT_TRUNCATION          "01004"
#define KIWI_WARNING_DEPRECATED_FEATURE                    "01P01"

/* Class 02 - No Data "this is also a warning class per the SQL standard" */
#define KIWI_NO_DATA                                    "02000"
#define KIWI_NO_ADDITIONAL_DYNAMIC_RESULT_SETS_RETURNED "02001"

/* Class 03 - SQL Statement Not Yet Complete */
#define KIWI_SQL_STATEMENT_NOT_YET_COMPLETE "03000"

/* Class 08 - Connection Exception */
#define KIWI_CONNECTION_EXCEPTION                              "08000"
#define KIWI_CONNECTION_DOES_NOT_EXIST                         "08003"
#define KIWI_CONNECTION_FAILURE                                "08006"
#define KIWI_SQLCLIENT_UNABLE_TO_ESTABLISH_SQLCONNECTION       "08001"
#define KIWI_SQLSERVER_REJECTED_ESTABLISHMENT_OF_SQLCONNECTION "08004"
#define KIWI_TRANSACTION_RESOLUTION_UNKNOWN                    "08007"
#define KIWI_PROTOCOL_VIOLATION                                "08P01"

/* Class 09 - Triggered Action Exception */
#define KIWI_TRIGGERED_ACTION_EXCEPTION "09000"

/* Class 0A - Feature Not Supported */
#define KIWI_FEATURE_NOT_SUPPORTED "0A000"

/* Class 0B - Invalid Transaction Initiation */
#define KIWI_INVALID_TRANSACTION_INITIATION "0B000"

/* Class 0F - Locator Exception */
#define KIWI_LOCATOR_EXCEPTION         "0F000"
#define KIWI_L_E_INVALID_SPECIFICATION "0F001"

/* Class 0L - Invalid Grantor */
#define KIWI_INVALID_GRANTOR         "0L000"
#define KIWI_INVALID_GRANT_OPERATION "0LP01"

/* Class 0P - Invalid Role Specification */
#define KIWI_INVALID_ROLE_SPECIFICATION "0P000"

/* Class 0Z - Diagnostics Exception */
#define KIWI_DIAGNOSTICS_EXCEPTION                               "0Z000"
#define KIWI_STACKED_DIAGNOSTICS_ACCESSED_WITHOUT_ACTIVE_HANDLER "0Z002"

/* Class 20 - Case Not Found */
#define KIWI_CASE_NOT_FOUND "20000"

/* Class 21 - Cardinality Violation */
#define KIWI_CARDINALITY_VIOLATION "21000"

/* Class 22 - Data Exception */
#define KIWI_DATA_EXCEPTION                             "22000"
#define KIWI_ARRAY_ELEMENT_ERROR                        "2202E"
#define KIWI_ARRAY_SUBSCRIPT_ERROR                      "2202E"
#define KIWI_CHARACTER_NOT_IN_REPERTOIRE                "22021"
#define KIWI_DATETIME_FIELD_OVERFLOW                    "22008"
#define KIWI_DATETIME_VALUE_OUT_OF_RANGE                "22008"
#define KIWI_DIVISION_BY_ZERO                           "22012"
#define KIWI_ERROR_IN_ASSIGNMENT                        "22005"
#define KIWI_ESCAPE_CHARACTER_CONFLICT                  "2200B"
#define KIWI_INDICATOR_OVERFLOW                         "22022"
#define KIWI_INTERVAL_FIELD_OVERFLOW                    "22015"
#define KIWI_INVALID_ARGUMENT_FOR_LOG                   "2201E"
#define KIWI_INVALID_ARGUMENT_FOR_NTILE                 "22014"
#define KIWI_INVALID_ARGUMENT_FOR_NTH_VALUE             "22016"
#define KIWI_INVALID_ARGUMENT_FOR_POWER_FUNCTION        "2201F"
#define KIWI_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION "2201G"
#define KIWI_INVALID_CHARACTER_VALUE_FOR_CAST           "22018"
#define KIWI_INVALID_DATETIME_FORMAT                    "22007"
#define KIWI_INVALID_ESCAPE_CHARACTER                   "22019"
#define KIWI_INVALID_ESCAPE_OCTET                       "2200D"
#define KIWI_INVALID_ESCAPE_SEQUENCE                    "22025"
#define KIWI_NONSTANDARD_USE_OF_ESCAPE_CHARACTER        "22P06"
#define KIWI_INVALID_INDICATOR_PARAMETER_VALUE          "22010"
#define KIWI_INVALID_PARAMETER_VALUE                    "22023"
#define KIWI_INVALID_REGULAR_EXPRESSION                 "2201B"
#define KIWI_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE          "2201W"
#define KIWI_INVALID_ROW_COUNT_IN_RESULT_OFFSET_CLAUSE  "2201X"
#define KIWI_INVALID_TABLESAMPLE_ARGUMENT               "2202H"
#define KIWI_INVALID_TABLESAMPLE_REPEAT                 "2202G"
#define KIWI_INVALID_TIME_ZONE_DISPLACEMENT_VALUE       "22009"
#define KIWI_INVALID_USE_OF_ESCAPE_CHARACTER            "2200C"
#define KIWI_MOST_SPECIFIC_TYPE_MISMATCH                "2200G"
#define KIWI_NULL_VALUE_NOT_ALLOWED                     "22004"
#define KIWI_NULL_VALUE_NO_INDICATOR_PARAMETER          "22002"
#define KIWI_NUMERIC_VALUE_OUT_OF_RANGE                 "22003"
#define KIWI_STRING_DATA_LENGTH_MISMATCH                "22026"
#define KIWI_STRING_DATA_RIGHT_TRUNCATION               "22001"
#define KIWI_SUBSTRING_ERROR                            "22011"
#define KIWI_TRIM_ERROR                                 "22027"
#define KIWI_UNTERMINATED_C_STRING                      "22024"
#define KIWI_ZERO_LENGTH_CHARACTER_STRING               "2200F"
#define KIWI_FLOATING_POINT_EXCEPTION                   "22P01"
#define KIWI_INVALID_TEXT_REPRESENTATION                "22P02"
#define KIWI_INVALID_BINARY_REPRESENTATION              "22P03"
#define KIWI_BAD_COPY_FILE_FORMAT                       "22P04"
#define KIWI_UNTRANSLATABLE_CHARACTER                   "22P05"
#define KIWI_NOT_AN_XML_DOCUMENT                        "2200L"
#define KIWI_INVALID_XML_DOCUMENT                       "2200M"
#define KIWI_INVALID_XML_CONTENT                        "2200N"
#define KIWI_INVALID_XML_COMMENT                        "2200S"
#define KIWI_INVALID_XML_PROCESSING_INSTRUCTION         "2200T"

/* Class 23 - Integrity Constraint Violation */
#define KIWI_INTEGRITY_CONSTRAINT_VIOLATION "23000"
#define KIWI_RESTRICT_VIOLATION             "23001"
#define KIWI_NOT_NULL_VIOLATION             "23502"
#define KIWI_FOREIGN_KEY_VIOLATION          "23503"
#define KIWI_UNIQUE_VIOLATION               "23505"
#define KIWI_CHECK_VIOLATION                "23514"
#define KIWI_EXCLUSION_VIOLATION            "23P01"

/* Class 24 - Invalid Cursor State */
#define KIWI_INVALID_CURSOR_STATE "24000"

/* Class 25 - Invalid Transaction State */
#define KIWI_INVALID_TRANSACTION_STATE                            "25000"
#define KIWI_ACTIVE_SQL_TRANSACTION                               "25001"
#define KIWI_BRANCH_TRANSACTION_ALREADY_ACTIVE                    "25002"
#define KIWI_HELD_CURSOR_REQUIRES_SAME_ISOLATION_LEVEL            "25008"
#define KIWI_INAPPROPRIATE_ACCESS_MODE_FOR_BRANCH_TRANSACTION     "25003"
#define KIWI_INAPPROPRIATE_ISOLATION_LEVEL_FOR_BRANCH_TRANSACTION "25004"
#define KIWI_NO_ACTIVE_SQL_TRANSACTION_FOR_BRANCH_TRANSACTION     "25005"
#define KIWI_READ_ONLY_SQL_TRANSACTION                            "25006"
#define KIWI_SCHEMA_AND_DATA_STATEMENT_MIXING_NOT_SUPPORTED       "25007"
#define KIWI_NO_ACTIVE_SQL_TRANSACTION                            "25P01"
#define KIWI_IN_FAILED_SQL_TRANSACTION                            "25P02"
#define KIWI_IDLE_IN_TRANSACTION_SESSION_TIMEOUT                  "25P03"

/* Class 26 - Invalid SQL Statement Name */
#define KIWI_INVALID_SQL_STATEMENT_NAME "26000"

/* Class 27 - Triggered Data Change Violation */
#define KIWI_TRIGGERED_DATA_CHANGE_VIOLATION "27000"

/* Class 28 - Invalid Authorization Specification */
#define KIWI_INVALID_AUTHORIZATION_SPECIFICATION "28000"
#define KIWI_INVALID_PASSWORD                    "28P01"

/* Class 2B - Dependent Privilege Descriptors Still Exist */
#define KIWI_DEPENDENT_PRIVILEGE_DESCRIPTORS_STILL_EXIST "2B000"
#define KIWI_DEPENDENT_OBJECTS_STILL_EXIST               "2BP01"

/* Class 2D - Invalid Transaction Termination */
#define KIWI_INVALID_TRANSACTION_TERMINATION "2D000"

/* Class 2F - SQL Routine Exception */
#define KIWI_SQL_ROUTINE_EXCEPTION                       "2F000"
#define KIWI_S_R_E_FUNCTION_EXECUTED_NO_RETURN_STATEMENT "2F005"
#define KIWI_S_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED      "2F002"
#define KIWI_S_R_E_PROHIBITED_SQL_STATEMENT_ATTEMPTED    "2F003"
#define KIWI_S_R_E_READING_SQL_DATA_NOT_PERMITTED        "2F004"

/* Class 34 - Invalid Cursor Name */
#define KIWI_INVALID_CURSOR_NAME "34000"

/* Class 38 - External Routine Exception */
#define KIWI_EXTERNAL_ROUTINE_EXCEPTION               "38000"
#define KIWI_E_R_E_CONTAINING_SQL_NOT_PERMITTED       "38001"
#define KIWI_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED   "38002"
#define KIWI_E_R_E_PROHIBITED_SQL_STATEMENT_ATTEMPTED "38003"
#define KIWI_E_R_E_READING_SQL_DATA_NOT_PERMITTED     "38004"

/* Class 39 - External Routine Invocation Exception */
#define KIWI_EXTERNAL_ROUTINE_INVOCATION_EXCEPTION   "39000"
#define KIWI_E_R_I_E_INVALID_SQLSTATE_RETURNED       "39001"
#define KIWI_E_R_I_E_NULL_VALUE_NOT_ALLOWED          "39004"
#define KIWI_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED       "39P01"
#define KIWI_E_R_I_E_SRF_PROTOCOL_VIOLATED           "39P02"
#define KIWI_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED "39P03"

/* Class 3B - Savepoint Exception */
#define KIWI_SAVEPOINT_EXCEPTION       "3B000"
#define KIWI_S_E_INVALID_SPECIFICATION "3B001"

/* Class 3D - Invalid Catalog Name */
#define KIWI_INVALID_CATALOG_NAME "3D000"

/* Class 3F - Invalid Schema Name */
#define KIWI_INVALID_SCHEMA_NAME "3F000"

/* Class 40 - Transaction Rollback */
#define KIWI_TRANSACTION_ROLLBACK               "40000"
#define KIWI_T_R_INTEGRITY_CONSTRAINT_VIOLATION "40002"
#define KIWI_T_R_SERIALIZATION_FAILURE          "40001"
#define KIWI_T_R_STATEMENT_COMPLETION_UNKNOWN   "40003"
#define KIWI_T_R_DEADLOCK_DETECTED              "40P01"

/* Class 42 - Syntax Error or Access Rule Violation */
#define KIWI_SYNTAX_ERROR_OR_ACCESS_RULE_VIOLATION "42000"
#define KIWI_SYNTAX_ERROR                          "42601"
#define KIWI_INSUFFICIENT_PRIVILEGE                "42501"
#define KIWI_CANNOT_COERCE                         "42846"
#define KIWI_GROUPING_ERROR                        "42803"
#define KIWI_WINDOWING_ERROR                       "42P20"
#define KIWI_INVALID_RECURSION                     "42P19"
#define KIWI_INVALID_FOREIGN_KEY                   "42830"
#define KIWI_INVALID_NAME                          "42602"
#define KIWI_NAME_TOO_LONG                         "42622"
#define KIWI_RESERVED_NAME                         "42939"
#define KIWI_DATATYPE_MISMATCH                     "42804"
#define KIWI_INDETERMINATE_DATATYPE                "42P18"
#define KIWI_COLLATION_MISMATCH                    "42P21"
#define KIWI_INDETERMINATE_COLLATION               "42P22"
#define KIWI_WRONG_OBJECT_TYPE                     "42809"
#define KIWI_UNDEFINED_COLUMN                      "42703"
#define KIWI_UNDEFINED_CURSOR                      "34000"
#define KIWI_UNDEFINED_DATABASE                    "3D000"
#define KIWI_UNDEFINED_FUNCTION                    "42883"
#define KIWI_UNDEFINED_PSTATEMENT                  "26000"
#define KIWI_UNDEFINED_SCHEMA                      "3F000"
#define KIWI_UNDEFINED_TABLE                       "42P01"
#define KIWI_UNDEFINED_PARAMETER                   "42P02"
#define KIWI_UNDEFINED_OBJECT                      "42704"
#define KIWI_DUPLICATE_COLUMN                      "42701"
#define KIWI_DUPLICATE_CURSOR                      "42P03"
#define KIWI_DUPLICATE_DATABASE                    "42P04"
#define KIWI_DUPLICATE_FUNCTION                    "42723"
#define KIWI_DUPLICATE_PSTATEMENT                  "42P05"
#define KIWI_DUPLICATE_SCHEMA                      "42P06"
#define KIWI_DUPLICATE_TABLE                       "42P07"
#define KIWI_DUPLICATE_ALIAS                       "42712"
#define KIWI_DUPLICATE_OBJECT                      "42710"
#define KIWI_AMBIGUOUS_COLUMN                      "42702"
#define KIWI_AMBIGUOUS_FUNCTION                    "42725"
#define KIWI_AMBIGUOUS_PARAMETER                   "42P08"
#define KIWI_AMBIGUOUS_ALIAS                       "42P09"
#define KIWI_INVALID_COLUMN_REFERENCE              "42P10"
#define KIWI_INVALID_COLUMN_DEFINITION             "42611"
#define KIWI_INVALID_CURSOR_DEFINITION             "42P11"
#define KIWI_INVALID_DATABASE_DEFINITION           "42P12"
#define KIWI_INVALID_FUNCTION_DEFINITION           "42P13"
#define KIWI_INVALID_PSTATEMENT_DEFINITION         "42P14"
#define KIWI_INVALID_SCHEMA_DEFINITION             "42P15"
#define KIWI_INVALID_TABLE_DEFINITION              "42P16"
#define KIWI_INVALID_OBJECT_DEFINITION             "42P17"

/* Class 44 - WITH CHECK OPTION Violation */
#define KIWI_WITH_CHECK_OPTION_VIOLATION "44000"

/* Class 53 - Insufficient Resources */
#define KIWI_INSUFFICIENT_RESOURCES       "53000"
#define KIWI_DISK_FULL                    "53100"
#define KIWI_OUT_OF_MEMORY                "53200"
#define KIWI_TOO_MANY_CONNECTIONS         "53300"
#define KIWI_CONFIGURATION_LIMIT_EXCEEDED "53400"

/* Class 54 - Program Limit Exceeded */
#define KIWI_PROGRAM_LIMIT_EXCEEDED "54000"
#define KIWI_STATEMENT_TOO_COMPLEX  "54001"
#define KIWI_TOO_MANY_COLUMNS       "54011"
#define KIWI_TOO_MANY_ARGUMENTS     "54023"

/* Class 55 - Object Not In Prerequisite State */
#define KIWI_OBJECT_NOT_IN_PREREQUISITE_STATE "55000"
#define KIWI_OBJECT_IN_USE                    "55006"
#define KIWI_CANT_CHANGE_RUNTIME_PARAM        "55P02"
#define KIWI_LOCK_NOT_AVAILABLE               "55P03"

/* Class 57 - Operator Intervention */
#define KIWI_OPERATOR_INTERVENTION "57000"
#define KIWI_QUERY_CANCELED        "57014"
#define KIWI_ADMIN_SHUTDOWN        "57P01"
#define KIWI_CRASH_SHUTDOWN        "57P02"
#define KIWI_CANNOT_CONNECT_NOW    "57P03"
#define KIWI_DATABASE_DROPPED      "57P04"

/* Class 58 - System Error "errors external to PostgreSQL itself" */
#define KIWI_SYSTEM_ERROR   "58000"
#define KIWI_IO_ERROR       "58030"
#define KIWI_UNDEFINED_FILE "58P01"
#define KIWI_DUPLICATE_FILE "58P02"

/* Class 72 - Snapshot Failure */
#define KIWI_SNAPSHOT_TOO_OLD "72000"

/* Class F0 - Configuration File Error */
#define KIWI_CONFIG_FILE_ERROR "F0000"
#define KIWI_LOCK_FILE_EXISTS  "F0001"

/* Class HV - Foreign Data Wrapper Error "SQL/MED" */
#define KIWI_FDW_ERROR                                  "HV000"
#define KIWI_FDW_COLUMN_NAME_NOT_FOUND                  "HV005"
#define KIWI_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED         "HV002"
#define KIWI_FDW_FUNCTION_SEQUENCE_ERROR                "HV010"
#define KIWI_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION    "HV021"
#define KIWI_FDW_INVALID_ATTRIBUTE_VALUE                "HV024"
#define KIWI_FDW_INVALID_COLUMN_NAME                    "HV007"
#define KIWI_FDW_INVALID_COLUMN_NUMBER                  "HV008"
#define KIWI_FDW_INVALID_DATA_TYPE                      "HV004"
#define KIWI_FDW_INVALID_DATA_TYPE_DESCRIPTORS          "HV006"
#define KIWI_FDW_INVALID_DESCRIPTOR_FIELD_IDENTIFIER    "HV091"
#define KIWI_FDW_INVALID_HANDLE                         "HV00B"
#define KIWI_FDW_INVALID_OPTION_INDEX                   "HV00C"
#define KIWI_FDW_INVALID_OPTION_NAME                    "HV00D"
#define KIWI_FDW_INVALID_STRING_LENGTH_OR_BUFFER_LENGTH "HV090"
#define KIWI_FDW_INVALID_STRING_FORMAT                  "HV00A"
#define KIWI_FDW_INVALID_USE_OF_NULL_POINTER            "HV009"
#define KIWI_FDW_TOO_MANY_HANDLES                       "HV014"
#define KIWI_FDW_OUT_OF_MEMORY                          "HV001"
#define KIWI_FDW_NO_SCHEMAS                             "HV00P"
#define KIWI_FDW_OPTION_NAME_NOT_FOUND                  "HV00J"
#define KIWI_FDW_REPLY_HANDLE                           "HV00K"
#define KIWI_FDW_SCHEMA_NOT_FOUND                       "HV00Q"
#define KIWI_FDW_TABLE_NOT_FOUND                        "HV00R"
#define KIWI_FDW_UNABLE_TO_CREATE_EXECUTION             "HV00L"
#define KIWI_FDW_UNABLE_TO_CREATE_REPLY                 "HV00M"
#define KIWI_FDW_UNABLE_TO_ESTABLISH_CONNECTION         "HV00N"

/* Class P0 - PL/pgSQL Error */
#define KIWI_PLPGSQL_ERROR   "P0000"
#define KIWI_RAISE_EXCEPTION "P0001"
#define KIWI_NO_DATA_FOUND   "P0002"
#define KIWI_TOO_MANY_ROWS   "P0003"
#define KIWI_ASSERT_FAILURE  "P0004"

/* Class XX - Internal Error */
#define KIWI_INTERNAL_ERROR  "XX000"
#define KIWI_DATA_CORRUPTED  "XX001"
#define KIWI_INDEX_CORRUPTED "XX002"

#endif /* KIWI_ERROR_CODES_H */
