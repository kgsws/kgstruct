#!/usr/bin/python3
import sys
import json
from collections import OrderedDict

comment_offsets = True

type_info_list = {
	"u8": {"ctype": "uint8_t", "stype": "KS_TYPEDEF_U8", "ktype": "kgstruct_u8_t", "min": 0, "max": 255},
	"u16": {"ctype": "uint16_t", "stype": "KS_TYPEDEF_U16", "ktype": "kgstruct_u16_t", "min": 0, "max": 65535},
	"u32": {"ctype": "uint32_t", "stype": "KS_TYPEDEF_U32", "ktype": "kgstruct_u32_t", "min": 0, "max": 4294967295},
	"u64": {"ctype": "uint64_t", "stype": "KS_TYPEDEF_U64", "ktype": "kgstruct_u64_t"},
	"s8": {"ctype": "int8_t", "stype": "KS_TYPEDEF_S8", "ktype": "kgstruct_s8_t", "min": -128, "max": 127},
	"s16": {"ctype": "int16_t", "stype": "KS_TYPEDEF_S16", "ktype": "kgstruct_s16_t", "min": -32768, "max": 32767},
	"s32": {"ctype": "int32_t", "stype": "KS_TYPEDEF_S32", "ktype": "kgstruct_s32_t", "min": -2147483648, "max": 2147483647},
	"s64": {"ctype": "int64_t", "stype": "KS_TYPEDEF_S64", "ktype": "kgstruct_s64_t"},
	"f32": {"ctype": "float", "stype": "KS_TYPEDEF_FLOAT", "ktype": "kgstruct_float_t"},
	"f64": {"ctype": "double", "stype": "KS_TYPEDEF_DOUBLE", "ktype": "kgstruct_double_t"}
}
type_padding = {"ctype": "uint8_t"}
type_c_string = {"ctype": "uint8_t", "stype": "KS_TYPEDEF_STRING", "ktype": "kgstruct_string_t"}
type_c_struct = {"stype": "KS_TYPEDEF_STRUCT", "ktype": "kgstruct_object_t"}
type_custom_base = {"stype": "KS_TYPEDEF_CUSTOM", "ktype": "kgstruct_custom_t"}

type_has_min = "KS_TYPEFLAG_HAS_MIN"
type_has_max = "KS_TYPEFLAG_HAS_MAX"
type_has_empty = "KS_TYPEFLAG_EMPTY_ARRAY"
type_is_bool = "KS_TYPEFLAG_IS_BOOL"

is_collapsed = True

export_types = []
type_custom_list = {}

##
def add_type(tdef):
	global export_types
	i = 0
	for name in export_types:
		if tdef == name:
			return i
		i += 1
	export_types.append(tdef)
	return i

#
# generate C and H file
def generate_code(infile, outname):
	global type_custom_list
	enable_fill_info = False
	# open and parse
	with open(infile, "rb") as f:
		data = json.loads(f.read().decode('utf8'), object_pairs_hook=OrderedDict)
		if "options" in data:
			config = data["options"]
			# type for 'string'
			if "string" in config:
				type_c_string["ctype"] = config["string"]
			# type for padding
			if "padding" in config:
				type_padding["ctype"] = config["padding"]
			# type for 'fill info' structures
			if "fill info" in config:
				enable_fill_info = config["fill info"]
		else:
			config = {}
		if "defs" in config:
			defs = config["defs"]
		else:
			defs = {}
		if "types" in config:
			type_custom_list = config["types"]
		if "include" in config:
			include_list = config["include"]
		else:
			include_list = {}
		structures = data["structures"]

	# parse all structures
	struct_list = OrderedDict()
	for struct_name in structures:
		struct = structures[struct_name]
		struct_template = OrderedDict()
		# parse structure elements
		for var_name in struct:
			var_info = struct[var_name]
			var_template = {}
			var_flags = "0"
			# type changes
			if var_info["type"] == "boolean":
				# modify the type
				var_info["type"] = config["boolean"]
				var_flags += "|" + type_is_bool
			# get element type
			if "padding" in var_info:
				# special case for padding
				type_info_def = dict(type_padding)
				var_template["padding"] = var_info["padding"]
				# optional padding type
				if "type" in var_info:
					type_info_def["ctype"] = type_info_list[var_info["type"]]["ctype"]
			elif var_info["type"] == "string":
				# special case for C strings
				type_info_def = dict(type_c_string)
				type_info_def["size"] = str(var_info["length"])
				var_template["string"] = str(var_info["length"])
			elif var_info["type"] in type_info_list:
				type_info_def = dict(type_info_list[var_info["type"]])
				# min / max
				if "min" in var_info:
					var_flags += "|" + type_has_min
					type_info_def["min"] = str(var_info["min"])
				if "max" in var_info:
					var_flags += "|" + type_has_max
					type_info_def["max"] = str(var_info["max"])
			else:
				if var_info["type"] in struct_list:
					# structure-in-structure
					type_info_def = dict(type_c_struct)
					type_info_def["ctype"] = var_info["type"] + "_t"
					type_info_def["size"] = "sizeof(%s_t)" % var_info["type"]
					type_info_def["struct"] = "ks_template__" + var_info["type"]
				elif var_info["type"] in type_custom_list:
					# custom type
					custom_type = type_custom_list[var_info["type"]]
					type_info_def = {}
					type_info_def["ctype"] = custom_type["type"]
					if "parser" in custom_type:
						type_info_def["parser"] = custom_type["parser"]
					if "exporter" in custom_type:
						type_info_def["exporter"] = custom_type["exporter"]
					type_info_def["stype"] = type_custom_base["stype"]
					type_info_def["ktype"] = type_custom_base["ktype"]
				else:
					# invalid type
					raise Exception("Unknown type name '%s' for '%s' in '%s'." % (var_info["type"], var_name, struct_name))
			# flags
			type_info_def["flags"] = var_flags
			# array check
			if "array" in var_info:
				if "empty" in var_info and var_info["empty"]:
					type_info_def["flags"] += "|" + type_has_empty
				var_template["array"] = var_info["array"]
				type_info_def["array"] = var_info["array"]
			# element 'key'
			if "key" in var_info:
				var_template["name"] = var_info["key"]
			else:
				var_template["name"] = var_name
			# type info
			var_template["ctype"] = type_info_def["ctype"]
			if "stype" in type_info_def:
				var_template["type_info_str"] = "ks_info_%u" % add_type(type_info_def)
				var_template["stype"] = type_info_def["stype"]
			# add this element
			struct_template[var_name] = var_template
		# add this structure
		struct_list[struct_name] = struct_template

	# generate H file
	output = open("%s.h" % outname, "w")
	output.write("// KGSTRUCT AUTO GENERATED FILE\n")
	# export all defs
	for def_name in defs:
		output.write("#define %s\t%u\n" % (def_name, defs[def_name]))
	# export all custom parsers
	for custom_name in type_custom_list:
		custom_info = type_custom_list[custom_name]
		if "parser" in custom_info:
			output.write("uint32_t %s(void *, const uint8_t *, uint32_t);\n" % custom_info["parser"])
		if "exporter" in custom_info:
			output.write("uint32_t %s(void *, uint8_t *);\n" % custom_info["exporter"])
	# export all structures
	for struct_name in struct_list:
		struct = struct_list[struct_name]
		# actual structure
		output.write("typedef struct %s_s\n{\n" % struct_name)
		# export all variables
		for var_name in struct:
			var_info = struct[var_name]
			output.write("\t%s %s" % (var_info["ctype"], var_name))
			if "padding" in var_info:
				output.write("[%u]" % var_info["padding"])
			elif "string" in var_info:
				if "array" in var_info:
					output.write("[%s][%s]" % (var_info["array"], var_info["string"]))
				else:
					output.write("[%s]" % var_info["string"])
			elif "array" in var_info:
				output.write("[%s]" % var_info["array"])
			output.write(";\n")
		# structure ending
		output.write("} %s_t;\n" % struct_name)
	# optional 'fill info' structures
	if enable_fill_info:
		output.write("#ifdef KGSTRUCT_FILLINFO_TYPE\n")
		# export all structures
		for struct_name in struct_list:
			struct = struct_list[struct_name]
			# special structure
			output.write("typedef struct %s_sf\n{\n" % struct_name)
			# export all variables
			for var_name in struct:
				var_info = struct[var_name]
				output.write("\tKGSTRUCT_FILLINFO_TYPE %s;\n" % var_name)
			# export extra variables
			for var_name in struct:
				var_info = struct[var_name]
				if "padding" in var_info:
					continue
				if var_info["stype"] == type_c_struct["stype"]:
					output.write("\t%sf __%s" % (var_info["ctype"], var_name))
					if "array" in var_info:
						output.write("[%s]" % var_info["array"])
					output.write(";\n")
			# structure ending
			output.write("} %s_tf;\n" % struct_name)
		# end
		output.write("#endif\n")
	# extra newline
	output.write("\n")
	# all the templates
	for struct_name in struct_list:
		output.write("extern const struct ks_base_template_s ks_template__%s;\n" % struct_name)
	# done
	output.close()

	# generate C file
	output = open("%s.c" % outname, "w")
	output.write("#include <stdint.h>\n")
	output.write("#include <stddef.h>\n")
	for value in include_list:
		output.write("#include \"%s\"\n" % value)
	output.write("#include \"kgstruct.h\"\n")
	output.write("#include \"%s.h\"\n" % outname)
	output.write("// KGSTRUCT AUTO GENERATED FILE\n")
	# generate types
	idx = 0
	for type_info in export_types:
		output.write("static const %s ks_info_%u =\n{\n" % (type_info["ktype"], idx))
		output.write("\t.base.type = %s,\n" % type_info["stype"])
		output.write("\t.base.flags = %s,\n" % type_info["flags"])
		if "size" in type_info:
			output.write("\t.base.size = %s,\n" % type_info["size"])
		if "array" in type_info:
			output.write("\t.base.array = %s,\n" % type_info["array"])
		if "struct" in type_info:
			output.write("\t.basetemp = &%s,\n" % type_info["struct"])
		if "parser" in type_info:
			output.write("\t.parse = %s,\n" % type_info["parser"])
		if "exporter" in type_info:
			output.write("\t.export = %s,\n" % type_info["exporter"])
		output.write("#ifdef KGSTRUCT_ENABLE_MINMAX\n")
		if "min" in type_info:
			output.write("\t.min = %s,\n" % type_info["min"])
		if "max" in type_info:
			output.write("\t.max = %s,\n" % type_info["max"])
		output.write("#endif\n")
		output.write("};\n")
		idx += 1
	# extra newline
	output.write("\n")
	# generate templates
	for struct_name in struct_list:
		struct = struct_list[struct_name]
		output.write("const ks_base_template_t ks_template__%s =\n{\n" % struct_name)
		# optional 'fill info'
		if enable_fill_info:
			output.write("#ifdef KGSTRUCT_FILLINFO_TYPE\n")
			output.write("\t.fill_size = sizeof(%s_tf),\n" % struct_name)
			output.write("#endif\n")
		# export all (visible) variables
		output.write("\t.template =\n\t{\n")
		for var_name in struct:
			var_info = struct[var_name]
			if "type_info_str" in var_info:
				output.write("\t\t{\n")
				output.write("\t\t\t.key = \"%s\",\n" % var_info["name"])
				output.write("\t\t\t.info = (kgstruct_type_t*)&%s,\n" % var_info["type_info_str"])
				output.write("\t\t\t.offset = offsetof(%s_t,%s),\n" % (struct_name, var_name))
				if enable_fill_info and var_info["stype"] == type_c_struct["stype"]:
					output.write("#ifdef KGSTRUCT_FILLINFO_TYPE\n")
					output.write("\t\t\t.fill_offs = offsetof(%s_tf,__%s),\n" % (struct_name, var_name))
					output.write("#endif\n")
				output.write("\t\t},\n")
		# terminator
		output.write("\t\t{}\n\t}\n};\n")
	# done
	output.close()

#
# generate JSON schema
def recursive_schema(structure_list, structure):
	global is_collapsed
	global defs
	struct_schema = OrderedDict()
	for var_name in structure:
		var_info = structure[var_name]
		var_type = OrderedDict()
		var_props = OrderedDict()
		var_options = {"collapsed": is_collapsed}
		# special skip
		if "padding" in var_info:
			continue
		# custom collapse
		if "collapsed" in var_info:
			var_options["collapsed"] = var_info["collapsed"]
		# type remap
		if var_info["type"] in type_custom_list:
			custom = type_custom_list[var_info["type"]]
			if "string" in custom and custom["string"]:
				var_type["type"] = "string"
			else:
				var_type["type"] = "integer"
		else:
			var_type["type"] = var_info["type"]
		# get element type
		if var_type["type"] == "string":
			# string
			if "length" in var_info:
				if isinstance(var_info["length"], str):
					var_props["maxLength"] = defs[var_info["length"]] - 1
				else:
					var_props["maxLength"] = var_info["length"] - 1
			if "pattern" in var_info:
				var_props["pattern"] = var_info["pattern"]
			if "enum" in var_info:
				var_props["enum"] = var_info["enum"]
		elif var_type["type"] == "boolean":
			# boolean
			if "enum" in var_info:
				# var_props["enum"] = [True, False] # is this a bug?
				var_options.update({"enum_titles": var_info["enum"]})
		elif var_type["type"] in type_info_list:
			# integer
			var_type["type"] = "integer"
			if "enum" in var_info:
				# custom range with names
				named_enum = var_info["enum"]
				var_props["enum"] = list(range(0, len(named_enum)))
				var_options.update({"enum_titles": var_info["enum"]})
			else:
				# range check by type
				range_type = type_info_list[var_info["type"]]
				if "min" in range_type:
					var_props["minimum"] = range_type["min"]
				if "max" in range_type:
					var_props["maximum"] = range_type["max"]
				# optional custom range
				if "min" in var_info:
					var_props["minimum"] = var_info["min"]
				if "max" in var_info:
					var_props["maximum"] = var_info["max"]
		else:
			# object
			var_type["type"] = "object"
		# basic element info
		if "title" in var_info:
			var_type["title"] = var_info["title"]
		if "description" in var_info:
			var_type["description"] = var_info["description"]
		# extra stuff
		if "format" in var_info:
			var_type["format"] = var_info["format"]
		# options export
		if var_type["type"] != "object" and not "array" in var_info:
			del var_options["collapsed"]
		if len(var_options):
			var_type["options"] = var_options
		# read-only hint
		if "readonly" in var_info and not "array" in var_info:
			var_type["readonly"] = var_info["readonly"]
		# not-array
		if not "array" in var_info:
			var_type.update(var_props)
		# dump object
		if var_type["type"] == "object":
			var_type["properties"] = recursive_schema(structure_list, structure_list[var_info["type"]])
		# array
		if "array" in var_info:
			arr_items = OrderedDict()
			# size
			if isinstance(var_info["array"], str):
				arr_size = defs[var_info["array"]]
			else:
				arr_size = var_info["array"]
			var_type["maxItems"] = arr_size
			if not "empty" in var_info or not var_info["empty"]:
				var_type["minItems"] = arr_size
			# change type
			arr_items["type"] = var_type["type"]
			var_type["type"] = "array"
			# add props
			arr_items.update(var_props)
			# schema additions
			if "array schema" in var_info:
				arr_items.update(var_info["array schema"])
			# reordering
			if "properties" in var_type:
				# array of objects
				arr_items["properties"] = var_type["properties"]
				del var_type["properties"]
			if "readonly" in var_info:
				arr_items["readonly"] = var_info["readonly"]
			var_type["items"] = arr_items
		# everything is required
		if "properties" in var_type:
			var_type["required"] = list(var_type["properties"].keys())
		if "items" in var_type:
			if var_type["items"]["type"] == "object":
				var_type["items"]["required"] = list(var_type["items"]["properties"].keys())
		# done
		if "key" in var_info:
			var_name = var_info["key"]
		struct_schema[var_name] = var_type
	return struct_schema

def generate_schema(infile, outfile, exportname):
	global defs
	global type_custom_list
	# open and parse
	with open(infile, "rb") as f:
		data = json.loads(f.read().decode('utf8'), object_pairs_hook=OrderedDict)
		structures = data["structures"]
		defs = {}
		config = {}
		config["title"] = " "
		config["format"] = "tabs"
		config["options"] = {"disable_collapse": True}
		if "options" in data:
			if "schema" in data["options"]:
				config.update(data["options"]["schema"])
			if "defs" in data["options"]:
				defs = data["options"]["defs"]
		if "types" in data["options"]:
			type_custom_list = data["options"]["types"]
	if not exportname in structures:
		raise Exception("Unknown schema struct '%s'!" % exportname)

	# setup schema
	schema = OrderedDict()
	schema["$schema"] = "http://json-schema.org/draft-04/schema#"
	schema["type"] = "object"
	schema["title"] = config["title"]
	schema["format"] = config["format"]
	schema["options"] = config["options"]

	# parse requested structure
	schema["properties"] = recursive_schema(structures, structures[exportname])

	# write file
	with open(outfile, "w") as f:
		f.write(json.dumps(schema, indent="\t"))

#
# MAIN

def usage_exit():
	print("usage:", sys.argv[0], "<-code | schema> template.json file_base")
	exit(1)

# args
if len(sys.argv) < 4:
	usage_exit()

if sys.argv[1] == "-code":
	generate_code(sys.argv[2], sys.argv[3])
else:
	generate_schema(sys.argv[2], sys.argv[3], sys.argv[1])

