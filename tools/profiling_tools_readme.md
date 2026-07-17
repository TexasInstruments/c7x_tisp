# TISP Profiling Tools

This directory contains tools for working with TISP profiling data. The NodeProfiler class in TISP now supports exporting profiling data to JSON format, which can then be converted to XLSX format for easier analysis on x86 machines.

## JSON Export Functionality

The `NodeProfiler` class now includes a `writeJsonResults()` function that exports profiling data to a JSON file. This function captures:

- Summary information (total time, standalone node time, supernode time, counts)
- Supernodes with their cycle counts
- Nodes within each supernode
- Standalone nodes (not part of any supernode)

### Usage Example

```cpp
// Initialize the profiler
NodeProfiler::initialize();

// Profile your code...
NodeProfiler::startNode();
// Code to profile
NodeProfiler::stopNode();

// Profile a supernode with contained nodes
NodeProfiler::startSuperNode();
NodeProfiler::startNode();
// Code to profile
NodeProfiler::stopNode();
NodeProfiler::stopSuperNode();

// Print the results to console (optional)
NodeProfiler::printResults();

// Export the results to a JSON file
const char* jsonFilename = "profiling_results.json";
NodeProfiler::writeJsonResults(jsonFilename);
```

## JSON to XLSX Converter

The `json_to_xlsx_converter.py` script converts the JSON profiling data to an Excel (XLSX) file for easier analysis. The script creates multiple sheets in the Excel file:

1. **Summary** - Overall profiling information
2. **Supernodes** - Overview of all supernodes
3. **Nodes** - Details of all nodes, including their parent supernode (if any)
4. **Supernode_X** - Individual sheets for each supernode (where X is the supernode ID), containing detailed node-level profiling data for that specific supernode
5. **Standalone_Nodes** - Details of nodes not part of any supernode

### Requirements

- Python 3.6 or higher
- openpyxl library (`pip install openpyxl`)

### Usage

```bash
python json_to_xlsx_converter.py input.json [output.xlsx]
```

If `output.xlsx` is not specified, the script will use the input filename with a `.xlsx` extension.

### Example

```bash
python json_to_xlsx_converter.py profiling_results.json
```

This will create `profiling_results.xlsx` with the profiling data organized in multiple sheets.

## Workflow

1. **On the C7x target**:
   - Profile your code using the NodeProfiler class
   - Export the profiling data to a JSON file using `NodeProfiler::writeJsonResults()`

2. **On the x86 machine**:
   - Transfer the JSON file from the C7x target to the x86 machine
   - Run the `json_to_xlsx_converter.py` script to convert the JSON data to XLSX format
   - Open the XLSX file in Excel or another spreadsheet application for analysis

## JSON Format

The JSON file has the following structure:

```json
{
  "summary": {
    "host_emulation": false,
    "total_time_cycles": 12345,
    "supernode_time_cycles": 5678,
    "standalone_node_time_cycles": 6789,
    "node_count": 10,
    "max_nodes": 500,
    "supernode_count": 2,
    "max_supernodes": 20
  },
  "supernodes": [
    {
      "id": 0,
      "cycles": 3456,
      "nodes": [
        {
          "id": 0,
          "cycles": 1234
        },
        {
          "id": 1,
          "cycles": 2222
        }
      ]
    },
    {
      "id": 1,
      "cycles": 2222,
      "nodes": [
        {
          "id": 2,
          "cycles": 1111
        },
        {
          "id": 3,
          "cycles": 1111
        }
      ]
    }
  ],
  "standalone_nodes": [
    {
      "id": 4,
      "cycles": 3333
    },
    {
      "id": 5,
      "cycles": 3456
    }
  ]
}
```

This format encapsulates each supernode with its node-level profiling data, making it easy to analyze the hierarchical relationship between supernodes and their contained nodes.
