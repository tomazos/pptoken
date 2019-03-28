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
    name = "pptoken",
    srcs = [
        "pptoken.cc",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [":pptoken_lib"],
)

cc_binary(
    name = "ppencode",
    srcs = [
        "ppencode.cc",
    ],
    linkopts = [
        "-pthread",
    ],
    deps = [":pptoken_lib"],
)

cc_binary(
    name = "ppsearch",
    srcs = [
        "ppsearch.cc",
    ],
    deps = [":pptoken_lib"],
)

cc_binary(
    name = "ppother",
    srcs = [
        "ppother.cc",
    ],
    deps = [":pptoken_lib"],
)
