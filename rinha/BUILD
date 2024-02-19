cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
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
        "@glog//:glog",
        "@simdjson//:simdjson",
        ":structs",
    ]
)
