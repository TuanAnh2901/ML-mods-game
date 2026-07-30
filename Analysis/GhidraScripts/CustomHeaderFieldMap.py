# @runtime PyGhidra

from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.address import Address
from ghidra.program.model.lang import Register
from ghidra.program.model.scalar import Scalar


HEADER_GLOBAL = "185f2e9f8"
WINDOW_INSTRUCTIONS = 48


def targets_header_global(instruction, header_global):
    for operand in range(instruction.getNumOperands()):
        rip_relative = False
        displacement = None
        for obj in instruction.getOpObjects(operand):
            if isinstance(obj, Address) and obj == header_global:
                return True
            if isinstance(obj, Register) and obj.getBaseRegister().getName() == "RIP":
                rip_relative = True
            if isinstance(obj, Scalar):
                displacement = obj

        if rip_relative and displacement is not None:
            target = instruction.getAddress().add(instruction.getLength())
            target = target.add(displacement.getSignedValue())
            if target == header_global:
                return True

    return False


header_global = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(HEADER_GLOBAL)
listing = currentProgram.getListing()
functions = currentProgram.getFunctionManager()
functions_by_entry = {}
function_ids = {}
ref_count = 0

print("HEADER_GLOBAL " + str(header_global))
for block in currentProgram.getMemory().getBlocks():
    if not block.isExecute():
        continue

    for source in listing.getInstructions(block, True):
        if monitor.isCancelled():
            break
        if not targets_header_global(source, header_global):
            continue

        function = functions.getFunctionContaining(source.getAddress())
        if function is None:
            continue

        entry = str(function.getEntryPoint())
        if entry not in function_ids:
            function_ids[entry] = len(function_ids) + 1
            functions_by_entry[entry] = function

        ref_count += 1
        function_id = function_ids[entry]
        print(
            "XREF {} FUNC {} {} {} AT {} {}".format(
                ref_count,
                function_id,
                entry,
                function.getName(),
                source.getAddress(),
                source,
            )
        )

        current = source
        for _ in range(WINDOW_INSTRUCTIONS):
            if current is None:
                break
            print("  {}  {}".format(current.getAddress(), current))
            current = current.getNext()

print("FUNCTION_COUNT " + str(len(function_ids)))
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
try:
    for entry, function_id in function_ids.items():
        if monitor.isCancelled():
            break

        function = functions_by_entry[entry]
        result = decompiler.decompileFunction(function, 60, monitor)
        print("DECOMP FUNC {} {} {}".format(function_id, entry, function.getName()))
        if result.decompileCompleted():
            print(result.getDecompiledFunction().getC())
        else:
            print("DECOMPILATION_FAILED " + result.getErrorMessage())
finally:
    decompiler.dispose()
