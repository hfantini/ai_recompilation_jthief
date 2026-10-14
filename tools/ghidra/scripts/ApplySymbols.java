// Applies the recovered names and types in tools/ghidra/symbols/jwlthief.symbols.
// Run as a headless -postScript, after FixupWin16.
// @category Win16

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.DataUtilities.ClearDataMode;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplySymbols extends GhidraScript {

	private static final CategoryPath GAME = new CategoryPath("/jwlthief");

	private Win16Api api;
	private int errors;

	@Override
	protected void run() throws Exception {
		Path file = getSourceFile().getParentFile().getParentFile().getFile(false).toPath()
				.resolve("symbols/jwlthief.symbols");
		api = new Win16Api(currentProgram);

		List<String> lines = Files.readAllLines(file);
		int applied = 0;
		for (int i = 0; i < lines.size(); i++) {
			String line = lines.get(i).strip();
			if (line.isEmpty() || line.startsWith("#")) {
				continue;
			}
			try {
				apply(line);
				applied++;
			}
			catch (Exception e) {
				printerr(file.getFileName() + ":" + (i + 1) + ": " + e.getMessage());
				errors++;
			}
		}
		println("Applied " + applied + " symbols, " + errors + " errors");
	}

	private void apply(String line) throws Exception {
		int hash = line.indexOf(" #");
		String comment = hash >= 0 ? line.substring(hash + 2).strip() : null;
		String declaration = hash >= 0 ? line.substring(0, hash).strip() : line;
		String[] fields = declaration.split("\\s+", 3);
		if (fields.length < 3) {
			throw new IllegalArgumentException("Incomplete line: " + line);
		}

		switch (fields[0]) {
			case "struct" -> api.defineStruct(GAME, fields[1], fields[2].split(","));
			case "fn" -> applyFunction(toAddr(fields[1]), fields[2], comment);
			case "var" -> {
				String[] nameAndType = fields[2].split("\\s+");
				if (nameAndType.length != 2) {
					throw new IllegalArgumentException("Expected NAME TYPE: " + fields[2]);
				}
				applyVariable(toAddr(fields[1]), nameAndType[0], nameAndType[1], comment);
			}
			default -> throw new IllegalArgumentException("Unknown kind: " + fields[0]);
		}
	}

	private void applyFunction(Address address, String name, String comment) throws Exception {
		Function function = getFunctionAt(address);
		if (function == null) {
			throw new IllegalArgumentException("No function at " + address);
		}
		function.setName(name, SourceType.USER_DEFINED);
		if (comment != null) {
			function.setComment(comment);
		}
	}

	private void applyVariable(Address address, String name, String typeSpec, String comment)
			throws Exception {
		DataType type = api.resolve(typeSpec);
		DataUtilities.createData(currentProgram, address, type, -1,
			ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
		createLabel(address, name, true, SourceType.USER_DEFINED);
		if (comment != null) {
			setEOLComment(address, comment);
		}
	}
}
