// Exports an analyzed program as plain text for bulk reading: decompiled C per
// function plus TSV indexes of functions, data-segment globals and strings.
// Run as a headless -postScript with the output directory as its argument.
// @category Win16

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.SegmentedAddress;
import ghidra.program.model.address.SegmentedAddressSpace;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;

public class ExportProgram extends GhidraScript {

	private static final int DECOMPILE_TIMEOUT_SECONDS = 120;
	private static final int MAX_VALUE_BYTES = 16;

	private final List<MemoryBlock> dataSegments = new ArrayList<>();
	// Data-segment address -> functions whose decompiled C mentions it.
	private final Map<Address, Set<String>> codeUsers = new TreeMap<>();

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length != 1) {
			throw new IllegalArgumentException("Usage: ExportProgram.java <output-dir>");
		}
		Path out = Path.of(args[0]);
		Files.createDirectories(out.resolve("functions"));

		for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
			if (isDataSegment(block)) {
				dataSegments.add(block);
			}
		}
		exportFunctions(out);
		exportGlobals(out);
		exportStrings(out);
	}

	private void exportFunctions(Path out) throws Exception {
		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);
		int failures = 0;
		int count = 0;
		try (PrintWriter index = tsv(out.resolve("functions.tsv"),
			"address", "name", "bytes", "thunk", "calling_convention", "signature", "callers",
			"callees")) {
			for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
				monitor.checkCancelled();
				String callers = names(function.getCallingFunctions(monitor));
				String callees = names(function.getCalledFunctions(monitor));
				row(index, function.getEntryPoint(), function.getName(true),
					function.getBody().getNumAddresses(), function.isThunk(),
					function.getCallingConventionName(),
					function.getSignature().getPrototypeString(), callers, callees);
				count++;

				// Imports have no body worth decompiling; the index row is enough.
				if (function.isThunk()) {
					continue;
				}
				DecompileResults results =
					decompiler.decompileFunction(function, DECOMPILE_TIMEOUT_SECONDS, monitor);
				String code;
				if (results.decompileCompleted()) {
					code = results.getDecompiledFunction().getC();
					recordDataUsage(function, code);
				}
				else {
					code = "/* DECOMPILE FAILED: " + results.getErrorMessage() + " */\n";
					failures++;
				}
				Files.writeString(out.resolve("functions").resolve(fileName(function)),
					"// " + function.getEntryPoint() + " " + function.getName(true) + "\n" +
						"// callers: " + callers + "\n" +
						"// callees: " + callees + "\n" + code);
			}
		}
		finally {
			decompiler.dispose();
		}
		println("Exported " + count + " functions, " + failures + " failed to decompile");
	}

	// Code passes data pointers as "push ds; push offset", which analysis does
	// not turn into references. The decompiler does resolve them into labels
	// (s_Jewel_Thief_1008_01d3) or far constants (0x10080058), so the C text is
	// the more complete source of who uses what.
	private void recordDataUsage(Function function, String code) {
		for (MemoryBlock block : dataSegments) {
			SegmentedAddress start = (SegmentedAddress) block.getStart();
			SegmentedAddressSpace space = (SegmentedAddressSpace) start.getAddressSpace();
			String segment = String.format("%04x", start.getSegment());
			Matcher matcher =
				Pattern.compile("(?:_" + segment + "_|\\b0x" + segment + ")([0-9a-f]{4})\\b")
						.matcher(code);
			while (matcher.find()) {
				Address address =
					space.getAddress(start.getSegment(), Integer.parseInt(matcher.group(1), 16));
				if (block.contains(address)) {
					codeUsers.computeIfAbsent(address, a -> new TreeSet<>())
							.add(function.getName(true));
				}
			}
		}
	}

	private void exportGlobals(Path out) throws Exception {
		ReferenceManager references = currentProgram.getReferenceManager();
		Set<Address> addresses = new TreeSet<>(codeUsers.keySet());
		for (MemoryBlock block : dataSegments) {
			references.getReferenceDestinationIterator(
				new AddressSet(block.getStart(), block.getEnd()), true).forEach(addresses::add);
		}

		try (PrintWriter index = tsv(out.resolve("globals.tsv"),
			"address", "label", "type", "bytes", "value", "used_by", "readers", "writers")) {
			for (Address address : addresses) {
				monitor.checkCancelled();
				Set<String> readers = new TreeSet<>();
				Set<String> writers = new TreeSet<>();
				for (Reference ref : references.getReferencesTo(address)) {
					if (ref.getReferenceType().isRead()) {
						readers.add(functionName(ref.getFromAddress()));
					}
					if (ref.getReferenceType().isWrite()) {
						writers.add(functionName(ref.getFromAddress()));
					}
				}
				Data data = getDataAt(address);
				int length = data != null ? data.getLength() : 0;
				row(index, address, label(address),
					data != null ? data.getDataType().getDisplayName() : "undefined", length,
					hexBytes(address, Math.max(length, 2)), String.join(",", users(address)),
					String.join(",", readers), String.join(",", writers));
			}
		}
		println("Exported " + addresses.size() + " globals");
	}

	// Strings outside the data segments are false positives found inside
	// resource data; real resource text comes from parsing the resources.
	private void exportStrings(Path out) throws Exception {
		int count = 0;
		try (PrintWriter index = tsv(out.resolve("strings.tsv"),
			"address", "label", "value", "used_by")) {
			for (MemoryBlock block : dataSegments) {
				for (Data data : currentProgram.getListing()
						.getDefinedData(new AddressSet(block.getStart(), block.getEnd()), true)) {
					monitor.checkCancelled();
					if (!data.hasStringValue()) {
						continue;
					}
					row(index, data.getAddress(), label(data.getAddress()),
						data.getDefaultValueRepresentation(),
						String.join(",", users(data.getAddress())));
					count++;
				}
			}
		}
		println("Exported " + count + " strings");
	}

	private Set<String> users(Address address) {
		Set<String> users = new TreeSet<>(codeUsers.getOrDefault(address, Set.of()));
		for (Reference ref : getReferencesTo(address)) {
			users.add(functionName(ref.getFromAddress()));
		}
		return users;
	}

	// The NE loader names segment blocks CodeN/DataN and resource blocks RsrcN.
	private static boolean isDataSegment(MemoryBlock block) {
		return block.isInitialized() && !block.isExecute() && block.getName().startsWith("Data");
	}

	private String functionName(Address address) {
		Function function = getFunctionContaining(address);
		return function != null ? function.getName(true) : address.toString();
	}

	private String label(Address address) {
		Symbol symbol = getSymbolAt(address);
		return symbol != null ? symbol.getName(true) : "";
	}

	private String hexBytes(Address address, int length) throws Exception {
		byte[] bytes = getBytes(address, Math.min(length, MAX_VALUE_BYTES));
		StringBuilder hex = new StringBuilder();
		for (byte b : bytes) {
			hex.append(String.format("%02x", b));
		}
		return length > MAX_VALUE_BYTES ? hex + "..." : hex.toString();
	}

	private static String names(Collection<Function> functions) {
		return functions.stream()
				.map(f -> f.getName(true))
				.sorted()
				.collect(Collectors.joining(","));
	}

	private static String fileName(Function function) {
		return function.getEntryPoint().toString().replace(':', '_') + "_" +
			function.getName().replaceAll("[^A-Za-z0-9_]", "_") + ".c";
	}

	private static PrintWriter tsv(Path file, String... header) throws IOException {
		PrintWriter writer = new PrintWriter(Files.newBufferedWriter(file));
		writer.println(String.join("\t", header));
		return writer;
	}

	private static void row(PrintWriter writer, Object... fields) {
		writer.println(Arrays.stream(fields)
				.map(field -> String.valueOf(field).replaceAll("[\t\r\n]", " "))
				.collect(Collectors.joining("\t")));
	}
}
