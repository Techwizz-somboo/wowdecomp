# Writes a C++ source holding every file under ROOT as a byte array, paths become Interface\GameEditor\...
file(GLOB_RECURSE files RELATIVE "${ROOT}" "${ROOT}/*")
list(SORT files)

set(body "#include \"HoT/Editor/Resources.h\"\n\nnamespace HoT::Editor::Resources {\n\n")
set(table "")
set(index 0)

foreach (relative ${files})
    file(READ "${ROOT}/${relative}" hex HEX)
    string(LENGTH "${hex}" length)

    if (length EQUAL 0)
        continue()
    endif ()

    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
    string(APPEND body "static const uint8_t s_file${index}[] = {\n${bytes}\n};\n\n")

    string(REPLACE "/" "\\\\" path "${relative}")
    string(APPEND table "    { \"Interface\\\\${path}\", s_file${index}, sizeof(s_file${index}) },\n")
    math(EXPR index "${index} + 1")
endforeach ()

string(APPEND body "extern const EmbeddedFile g_embeddedFiles[] = {\n${table}};\n\n")
string(APPEND body "extern const size_t g_embeddedFileCount = ${index};\n\n}\n")

file(WRITE "${OUTPUT}" "${body}")
