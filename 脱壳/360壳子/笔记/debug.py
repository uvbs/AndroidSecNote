import idautils
import idc
import idaapi
import time

#jni_Onload_offset = 0x1D6B50
jni_Onload_offset = 0x1D545E
RegisterNative_offset = 0x1B6610
OpenMemory_offset = 0xFC278 

def getModuleBase(moduleName):
    base = GetFirstModule()
    while (base != None) and (GetModuleName(base).find(moduleName) == -1):
        base = GetNextModule(base)
    if base == None:
        print "failed to find module: " + moduleName 
        return None
    else:
        return base

def addBrk(moduleName, functin_offset):
    base = getModuleBase(moduleName)
    AddBpt(base + functin_offset)


def fn_f7():
    idaapi.step_into()
    GetDebuggerEvent(WFNE_SUSP | WFNE_SUSP, -1) 

def fn_f8():
    idaapi.step_over()
    GetDebuggerEvent(WFNE_SUSP | WFNE_CONT, -1) 

def fn_f9():
    idaapi.continue_process()
    GetDebuggerEvent(WFNE_SUSP | WFNE_CONT, -1) 

def delBrk(moduleName, function_offset):
    base = getModuleBase(moduleName)
    DelBpt(base + function_offset)

def antiDebug():
    addBrk("libart.so", jni_Onload_offset)
    fn_f9()
    delBrk("libart.so", jni_Onload_offset)
    #into libjiagu.so
    fn_f7()

    #add breakpoint at time
    addBrk("libjiagu.so", 0x19FC)
    fn_f9()
    delBrk("libjiagu.so", 0x19FC)
    print("first call time")
    lr = GetRegValue("LR")
    AddBpt(lr)
    fn_f9()
    r0 = GetRegValue("R0")
    DelBpt(lr)
    print("first get time: %x" % (r0))

    #add breakpoint at sub_1E9C 
    addBrk("libjiagu.so", 0x1E9c)
    fn_f9()
    delBrk("libjiagu.so", 0x1E9c)
    print("call sub_1E9C")
    #add breakpoint at return from sub_1E9C
    lr = GetRegValue("LR")
    AddBpt(lr)
    fn_f9()
    DelBpt(lr)
    #get rtld_db_dlactivity address
    r0 = GetRegValue("R0")
    print("rtld_db_dlactivity address is: %x" % (r0 - 1))
    #set rtld_db_dlactivity nop
    PatchDword(r0 - 1, 0x46c0)

    #add breakpoint at strtol
    addBrk("libjiagu.so", 0x1A80)
    fn_f9()
    delBrk("libjiagu.so", 0x1A80)
    print("call strtol")
    lr = GetRegValue("LR")
    #add breakpoint at return from strtol
    AddBpt(lr)
    fn_f9()
    DelBpt(lr)
    r0 = GetRegValue("R0")
    print("get trace id: %x" % (r0))
    SetRegValue(0, "R0")

    #add break point at sub_6DD0
    addBrk("libjiagu.so", 0x6DD0)
    fn_f9()
    lr = GetRegValue("LR")
    print("call sub_6DD0")
    delBrk("libjiagu.so", 0x6DD0)
    AddBpt(lr)
    fn_f9()
    SetRegValue(0, "R0")
    DelBpt(lr)

    #add break point at time in  sub_6EF8
    addBrk("libjiagu.so", 0x19FC)
    fn_f9()
    delBrk("libjiagu.so", 0x19FC)
    print("second call time")
    lr = GetRegValue("LR")
    AddBpt(lr)
    fn_f9()
    DelBpt(lr)
    r0 = GetRegValue("R0")
    print("second get time: %x" % (r0))
    #SetRegValue(r0 + 2, "R0")


def get_string(addr):
  out = ""
  while True:
    if Byte(addr) != 0:
      out += chr(Byte(addr))
    else:
      break
    addr += 1
  return out


def hook_RegisterNative():
    #add break at RegisterNatives
    addBrk("libart.so", RegisterNative_offset)
    fn_f9()
    delBrk("libart.so", RegisterNative_offset)
    #JNINativeMethod method[]
    r2 = GetRegValue("R2")
    #nMethods
    r3 = GetRegValue("R3")
    for index in range(r3):
        name = get_string(Dword(r2))
        address = Dword(r2 + 8)
        #AddBpt(address - 1)
        print("native function %s address is %x" % (name, address))
        r2 = r2 + 12


def dump_dex(start_address, size):
    data = idaapi.dbg_read_memory(start_address, size)
    fp = open('E:\\dump.dex', 'wb')
    fp.write(data)
    fp.close()

#sub_6868
def decryptso(data, size):
    result = bytearray(data)
    for i in range(size):
        result[i] ^= 0x50
    return str(result)


def dump_so(start_address, size):
    fp = open("E:\\dump.so", "wb")

    #read elf header and decrypt header
    data = idaapi.dbg_read_memory(start_address, 52)
    header = decryptso(data, 52)
    fp.write(header)

    #read elf program header tables
    phnum = 9
    data = idaapi.dbg_read_memory(start_address + 52,  phnum * 32)
    pht = decryptso(data, phnum * 32)
    fp.write(pht)

    #read other part
    data = data = idaapi.dbg_read_memory(start_address + 52 + phnum * 32,  size - phnum * 32 - 52)
    fp.write(data)
    
    fp.close()


def main():

    antiDebug()

    #add break at sub_273c
    addBrk("libjiagu.so", 0x273C)
    fn_f9()
    base = GetRegValue("R0")
    print("second so uncompress at: %x" % (base))
    delBrk("libjiagu.so", 0x273C)
    

    #add break at sub_3DBC
    addBrk("libjiagu.so", 0x3DBC)
    fn_f9()
    r0 = GetRegValue("R0")
    print("encrypted so at: %x" % Dword(r0))
    print("encrypted so size: %x" % Dword(r0 + 4))
    print("uncompressed so at: %x" % Dword(r0 + 8))
    print("uncompressed so size: %x" % Dword(r0 + 12)) 
    delBrk("libjiagu.so", 0x3DBC)

    #dump_so(Dword(r0 + 8), Dword(r0 + 12))
    
    
    #add beak at case35
    addBrk("libjiagu.so", 0xAB84)
    fn_f9()
    delBrk("libjiagu.so", 0xAB84)
    lr = GetRegValue("LR")
    print("step into second so JNI_Onload: %x" % (lr - 1))

    fn_f7()

    hook_RegisterNative()  

    #add break at OpenMemory
    addBrk("libart.so", OpenMemory_offset)
    fn_f9()
    r0 = GetRegValue("R0")
    r1 = GetRegValue("R1")
    print("orgin dex at: %x" % (r0))
    delBrk("libart.so", OpenMemory_offset)
    #dump_dex(r0, r1)
    
    hook_RegisterNative()

    hook_RegisterNative()

    hook_RegisterNative()

    hook_RegisterNative()
   



main()