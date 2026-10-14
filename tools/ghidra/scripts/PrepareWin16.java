// Prepares a freshly imported Microsoft C Win16 (NE) program for auto-analysis.
// Run as a headless -preScript.
// @category Win16

import java.nio.file.Path;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.listing.Function;

public class PrepareWin16 extends GhidraScript {

	@Override
	protected void run() throws Exception {
		// Off by default because it is slow on large programs; this one is tiny,
		// and without it most functions decompile with no parameters.
		setAnalysisOption(currentProgram, "Decompiler Parameter ID", "true");

		defineSwitchTables();
		// Before analysis, so that parameter ID propagates the API types.
		applyApiPrototypes();
	}

	private void defineSwitchTables() throws Exception {
		for (Win16SwitchTables.SwitchTable sw : Win16SwitchTables.find(currentProgram,
			monitor)) {
			// Defined data keeps the disassembler from decoding the inline table
			// as instructions and following bogus flows from it.
			int entries = sw.targets().size();
			createData(sw.table(), new ArrayDataType(WordDataType.dataType, entries, 2));
			println("Switch table at " + sw.table() + ": " + entries + " entries");
		}
	}

	private void applyApiPrototypes() throws Exception {
		Path header = getSourceFile().getParentFile().getParentFile().getFile(false).toPath()
				.resolve("win16/win16api.h");
		Map<String, Win16Api.Prototype> prototypes = new HashMap<>();
		for (Win16Api.Prototype prototype : Win16Api.parseHeader(header)) {
			prototypes.put(prototype.name().toUpperCase(Locale.ROOT), prototype);
		}

		// NE import names are upper case; the header keeps the SDK spelling.
		Win16Api api = new Win16Api(currentProgram);
		Set<String> unused = new TreeSet<>(prototypes.keySet());
		for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
			if (!function.isThunk()) {
				continue;
			}
			String name = function.getName().toUpperCase(Locale.ROOT);
			Win16Api.Prototype prototype = prototypes.get(name);
			if (prototype == null) {
				println("No prototype for import " + function.getName(true));
				continue;
			}
			api.apply(function, prototype);
			unused.remove(name);
		}
		println("Applied " + (prototypes.size() - unused.size()) + " API prototypes");
		if (!unused.isEmpty()) {
			printerr("Prototypes matching no import: " + unused);
		}
	}
}
