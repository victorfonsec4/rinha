cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "@threadpool//:threadpool",
        ":request_handler",
        ":sqlite_database",
    ]
)


cc_library(
    name = "structs",
    srcs = ["structs.h"],
    deps = [
    ]
)

cc_library(
    name = "in_memory_database",
    hdrs = ["in_memory_database.h"],
    srcs = ["in_memory_database.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":structs",
    ]
)

cc_library(
    name = "sqlite_database",
    hdrs = ["sqlite_database.h"],
    srcs = ["sqlite_database.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "@sqlite3//:sqlite3",
        ":structs",
        ":to_json",
    ]
)

cc_library(
    name = "to_json",
    hdrs = ["to_json.h"],
    srcs = ["to_json.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":structs",
    ]
)

cc_library(
    name = "from_http",
    hdrs = ["from_http.h"],
    srcs = ["from_http.cc"],
    deps = [
        "@abseil//:abseil",
        "@abseil//:abseil-absl_time",
        "@glog//:glog",
        "@simdjson//:simdjson",
        ":structs",
    ]
)

cc_library(
    name = "request_handler",
    hdrs = ["request_handler.h"],
    srcs = ["request_handler.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":structs",
        ":in_memory_database",
        ":to_json",
        ":from_http",
        ":sqlite_database",
    ]
)
