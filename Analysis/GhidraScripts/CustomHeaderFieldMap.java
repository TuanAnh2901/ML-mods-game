import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.scalar.Scalar;

import java.util.LinkedHashMap;
import java.util.Map;

public class CustomHeaderFieldMap extends GhidraScript {
    private static final String HEADER_GLOBAL = "185f2e9f8";
    private static final int WINDOW_INSTRUCTIONS = 32;

    private boolean targetsHeaderGlobal(Instruction instruction, Address headerGlobal) {
        for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
            boolean ripRelative = false;
            Scalar displacement = null;
            for (Object object : instruction.getOpObjects(operand)) {
                if (object instanceof Address && object.equals(headerGlobal)) {
                    return true;
                }
                if (object instanceof Register
                    && ((Register) object).getBaseRegister().getName().equals("RIP")) {
                    ripRelative = true;
                }
                if (object instanceof Scalar) {
                    displacement = (Scalar) object;
                }
            }

            if (ripRelative && displacement != null) {
                Address target = instruction.getAddress().add(instruction.getLength())
                    .add(displacement.getSignedValue());
                if (target.equals(headerGlobal)) {
                    return true;
                }
            }
        }
        return false;
    }

    @Override
    public void run() throws Exception {
        Address headerGlobal = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(HEADER_GLOBAL);
        Listing listing = currentProgram.getListing();
        FunctionManager functions = currentProgram.getFunctionManager();
        Map<Address, Function> referencedFunctions = new LinkedHashMap<>();
        int referenceCount = 0;

        println("HEADER_GLOBAL " + headerGlobal);
        InstructionIterator instructions = listing.getInstructions(true);
        while (instructions.hasNext()) {
            if (monitor.isCancelled()) {
                break;
            }

            Instruction source = instructions.next();
            if (!targetsHeaderGlobal(source, headerGlobal)) {
                continue;
            }

            referenceCount++;
            Function function = functions.getFunctionContaining(source.getAddress());
            println("XREF " + referenceCount + " " + source.getAddress() + " " + source
                + " FUNC " + (function == null
                    ? "<none>"
                    : function.getName() + " @ " + function.getEntryPoint()));

            Instruction current = source;
            for (int i = 0; i < WINDOW_INSTRUCTIONS && current != null; i++) {
                println("  " + current.getAddress() + "  " + current);
                current = current.getNext();
            }

            if (function != null) {
                referencedFunctions.putIfAbsent(function.getEntryPoint(), function);
            }
        }

        println("REFERENCE_COUNT " + referenceCount);
        println("FUNCTION_COUNT " + referencedFunctions.size());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (Map.Entry<Address, Function> entry : referencedFunctions.entrySet()) {
                if (monitor.isCancelled()) {
                    break;
                }

                Function function = entry.getValue();
                println("DECOMP " + function.getName() + " @ " + entry.getKey());
                DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
                if (result.decompileCompleted()) {
                    println(result.getDecompiledFunction().getC());
                } else {
                    println("DECOMPILATION_FAILED " + result.getErrorMessage());
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
