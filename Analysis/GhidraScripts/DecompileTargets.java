import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class DecompileTargets extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager functions = currentProgram.getFunctionManager();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (String argument : getScriptArgs()) {
                if (monitor.isCancelled()) {
                    break;
                }

                Address address = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(argument);
                Function function = functions.getFunctionAt(address);
                if (function == null) {
                    function = functions.getFunctionContaining(address);
                }

                println("TARGET " + address);
                if (function == null) {
                    println("FUNCTION_NOT_FOUND");
                    continue;
                }

                println("FUNCTION " + function.getName() + " @ " + function.getEntryPoint());
                DecompileResults result = decompiler.decompileFunction(function, 180, monitor);
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
