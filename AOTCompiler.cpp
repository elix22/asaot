#include "AOTCompiler.h"


AOTCompiler::AOTCompiler(AOTLinkerEntry *linkerTable, unsigned int linkerTableSize)
: m_linkerTable(linkerTable), m_linkerTableSize(linkerTableSize)
{
}

AOTCompiler::~AOTCompiler()
{
}

int AOTCompiler::CompileFunction(asIScriptFunction *function, asJITFunction *output)
{
    asUINT   length;
    asDWORD *byteCode = function->GetByteCode(&length);
    asUINT   offset = 0;
    asDWORD *end = byteCode + length;
    AOTFunction f;

    if (function->GetModuleName())
        f.m_name += function->GetModuleName();
    if (function->GetNamespace())
        f.m_name += function->GetNamespace();
    if (function->GetScriptSectionName())
        f.m_name += function->GetScriptSectionName();
    if (function->GetObjectName())
        f.m_name += function->GetObjectName();
    if (function->GetName())
        f.m_name += function->GetName();

    if (m_linkerTable && m_linkerTableSize)
    {
        for (unsigned int i = 0; i < m_linkerTableSize; i++)
        {
            if (f.m_name == m_linkerTable[i].name)
            {
                f.m_entry = m_linkerTable[i].entry;
            }
        }
    }

    while ( byteCode < end )
    {
        asEBCInstr op = asEBCInstr(*(asBYTE*)byteCode);
        char buf[128];
        if (f.m_entry == 0)
        {
            snprintf(buf, 128, "bytecodeoffset_%d:\n", offset);
            f.m_output += buf;
        }
        if (op == asBC_JitEntry)
        {
            snprintf(buf, 128, "        case %d:\n", ++f.m_labelCount);
            f.m_output += buf;
            asBC_PTRARG(byteCode) = f.m_labelCount;
        }
        else if (f.m_entry == 0)
        {
            ProcessByteCode(byteCode, offset, op, f);
        }

        byteCode += asBCTypeSize[asBCInfo[op].type];
        offset   += asBCTypeSize[asBCInfo[op].type];
    }
    m_functions.push_back(f);

    if (f.m_entry != 0)
    {
        *output = f.m_entry;
        return asSUCCESS;
    }

    return asERROR;
}

void AOTCompiler::DumpCode()
{
    std::string output;
    output += "#include <as_config.h>\n";
    output += "#include <as_context.h>\n";
    output += "#include <as_scriptengine.h>\n";
    output += "#include <as_callfunc.h>\n";
    output += "#include <as_scriptobject.h>\n";
    output += "#include <as_texts.h>\n";
    output += "#include <AOTLinker.h>\n";
    // TODO: is there a better way to handle this? What if it changes?
    output += "\nstatic const int CALLSTACK_FRAME_SIZE = 5;\n\n";

    for (std::vector<AOTFunction>::iterator i = m_functions.begin(); i < m_functions.end(); i++)
    {
        output += "void ";
        output += (*i).m_name;
        output += "(asSVMRegisters * registers, asPWORD jitArg)\n";
        output += "{\n";
        output += "    printf(\"In aot compiled function!\\n\");\n";
        output += "    asDWORD * l_bc = registers->programPointer;\n";
        output += "    asDWORD * l_sp = registers->stackPointer;\n";
        output += "    asDWORD * l_fp = registers->stackFramePointer;\n";
        output += "    asCContext * context = (asCContext*) registers->ctx;\n";
        output += "    switch (jitArg)\n";
        output += "    {\n";
        output += (*i).m_output;
        output += "     }\n";
        output += "}\n";
    }
    char buf[512];
    snprintf(buf, 512, "\nunsigned int AOTLinkerTableSize = %d;\n", (int) m_functions.size());

    output += buf;
    output += "AOTLinkerEntry AOTLinkerTable[] =\n{\n";
    for (std::vector<AOTFunction>::iterator i = m_functions.begin(); i < m_functions.end(); i++)
    {
        snprintf(buf, 512, "{\"%s\", %s},\n", (*i).m_name.c_str(), (*i).m_name.c_str());
        output += buf;
    }
    output += "};\n";

    FILE *fp = fopen("aot_generated_code.cpp", "w");
    fprintf(fp, output.c_str());
    fclose(fp);
}

void AOTCompiler::ReleaseJITFunction(asJITFunction func)
{

}
