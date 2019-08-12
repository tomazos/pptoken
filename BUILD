cc_library(
    name = "pptoken_lib",
    srcs = [
        "text.cc",
        "tokenize.cc",
    ],
    hdrs = [
        "index.h",
        "index_reader.h",
        "mmapfile.h",
        "ppsearch.h",
        "text.h",
        "token_codec.h",
        "token_stream.h",
        "tokenize.h",
        "vector_token_stream.h",
    ],
    deps = [
        "//dvc:file",
        "//dvc:program",
        "//dvc:string",
    ],
)

cc_test(
    name = "token_codec_test",
    srcs = [
        "token_codec_test.cc",
    ],
    deps = [
        ":pptoken_lib",
    ],
)

cc_binary(
    name = "ppindex",
    srcs = [
        "ppindex.cc",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [
        ":pptoken_lib",
        "//dvc:math",
        "//dvc:sha3",
    ],
)

cc_binary(
    name = "ppverify",
    srcs = [
        "ppverify.cc",
    ],
    deps = [
        ":pptoken_lib",
    ],
)

cc_binary(
    name = "pp1gram",
    srcs = [
        "pp1gram.cc",
    ],
    deps = [
        ":pptoken_lib",
    ],
)

cc_binary(
    name = "cli_ppsearch",
    srcs = [
        "cli_ppsearch.cc",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [
        ":pptoken_lib",
        "//dvc:sampler",
    ],
)

cc_binary(
    name = "cgi_ppsearch",
    srcs = [
        "cgi_ppsearch.cc",
    ],
    linkopts = [
        "-lcgic",
        "-pthread",
    ],
    deps = [
        ":pptoken_lib",
        "//dvc:sampler",
    ],
)
