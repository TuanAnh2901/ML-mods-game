import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;

public class RuntimeInitTriage extends GhidraScript {
    private static final String RUNTIME_INIT = "1804268d0";

    @Override
    public void run() throws Exception {
        Address entry = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(RUNTIME_INIT);
        FunctionManager functions = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();
        Function function = functions.getFunctionAt(entry);

        println("PROGRAM " + currentProgram.getName());
        println("RUNTIME_INIT " + entry);
        if (function == null) {
            println("NO_FUNCTION");
            return;
        }
        println("FUNCTION " + function.getName() + " BODY " + function.getBody());
        println("=== DIRECT_CALLS ===");
        for (Instruction instruction : listing.getInstructions(function.getBody(), true)) {
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Function callee = functions.getFunctionAt(reference.getToAddress());
                println(instruction.getAddress() + " -> " + reference.getToAddress()
                    + " " + (callee == null ? "<unknown>" : callee.getName()));
            }
        }
        println("=== DECOMPILE ===");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
        if (!result.decompileCompleted()) {
            println("DECOMPILATION_FAILED " + result.getErrorMessage());
        } else {
            println(result.getDecompiledFunction().getC());
        }
        decompiler.dispose();
    }
}
