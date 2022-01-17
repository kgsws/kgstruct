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
type_time_split = {"ctype": "kgstruct_time_t", "stype": "KS_TYPEDEF_TIME_SPLIT", "ktype": "kgstruct_base_only_t"}
type_time_mult = {"ctype": "uint32_t", "stype": "KS_TYPEDEF_TIME_MULT", "ktype": "kgstruct_base_only_t"}
type_c_string = {"ctype": "uint8_t", "stype": "KS_TYPEDEF_STRING", "ktype": "kgstruct_string_t"}
type_c_struct = {"stype": "KS_TYPEDEF_STRUCT", "ktype": "kgstruct_object_t"}

type_has_min = "KS_TYPEFLAG_HAS_MIN"
type_has_max = "KS_TYPEFLAG_HAS_MAX"
type_has_seconds = "KS_TYPEFLAG_HAS_SECONDS"

is_collapsed = True

export_types = []

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
	# open and parse
	with open(infile, "rb") as f:
		data = json.loads(f.read().decode('utf8'), object_pairs_hook=OrderedDict)
		if "options" in data:
			config = data["options"]
			# string type
			if "string" in config:
				type_c_string["ctype"] = config["string"]
			if "padding" in config:
				type_padding["ctype"] = config["padding"]
		else:
			config = {}
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
			# get element type
			if "padding" in var_info:
				# special case for padding
				type_info_def = dict(type_padding)
				var_template["padding"] = var_info["padding"]
				# optional padding type
				if "type" in var_info:
					var_template["ctype"] = type_info_list[var_info["type"]]["ctype"]
			elif var_info["type"] == "string":
				# special case for C strings
				type_info_def = dict(type_c_string)
				type_info_def["size"] = str(var_info["length"])
				var_template["string"] = var_info["length"]
			elif var_info["type"] == "time split":
				# special case for 'time' in "H:M:S" format
				type_info_def = dict(type_time_split)
				if "seconds" in var_info and var_info["seconds"]:
					var_flags += "|" + type_has_seconds
			elif var_info["type"] == "time":
				# special case for 'time' in "H * 3600000 + M * 60000 + S * 1000" format
				type_info_def = dict(type_time_mult)
				if "seconds" in var_info and var_info["seconds"]:
					var_flags += "|" + type_has_seconds
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
				else:
					# invalid type
					raise Exception("Unknown type name '%s' for '%s' in '%s'." % (var_info["type"], var_name, struct_name))
			# flags
			type_info_def["flags"] = var_flags
			# array check
			if "array" in var_info:
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
			# add this element
			struct_template[var_name] = var_template
		# add this structure
		struct_list[struct_name] = struct_template

	# generate H file
	output = open("%s.h" % outname, "w")
	output.write("// KGSTRUCT AUTO GENERATED FILE\n")
	# export all structures
	for struct_name in struct_list:
		struct = struct_list[struct_name]
		output.write("typedef struct %s_s\n{\n" % struct_name)
		# export all variables
		for var_name in struct:
			var_info = struct[var_name]
			output.write("\t%s %s" % (var_info["ctype"], var_name))
			if "padding" in var_info:
				output.write("[%u]" % var_info["padding"])
			elif "string" in var_info:
				if "array" in var_info:
					output.write("[%u][%u]" % (var_info["array"], var_info["string"]))
				else:
					output.write("[%u]" % var_info["string"])
			elif "array" in var_info:
				output.write("[%u]" % var_info["array"])
			output.write(";\n")
		# structure ending
		output.write("} %s_t;\n" % struct_name)
	# extra newline
	output.write("\n")
	# all the templates
	for struct_name in struct_list:
		output.write("extern const ks_template_t ks_template__%s[];\n" % struct_name)
	# done
	output.close()

	# generate C file
	output = open("%s.c" % outname, "w")
	output.write("#include <stdint.h>\n")
	output.write("#include <stddef.h>\n")
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
			output.write("\t.template = %s,\n" % type_info["struct"])
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
		output.write("const ks_template_t ks_template__%s[] =\n{\n" % struct_name)
		# export all (visible) variables
		for var_name in struct:
			var_info = struct[var_name]
			if "type_info_str" in var_info:
				output.write("\t{\n")
				output.write("\t\t.key = \"%s\",\n" % var_info["name"])
				output.write("\t\t.info = (kgstruct_type_t*)&%s,\n" % var_info["type_info_str"])
				output.write("\t\t.offset = offsetof(%s_t,%s),\n" % (struct_name, var_name))
				output.write("\t},\n")
		# terminator
		output.write("\t{}\n};\n")
	# done
	output.close()

#
# generate JSON schema
def recursive_schema(structure_list, structure):
	global is_collapsed
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
		if var_info["type"] == "time":
			var_type["type"] = "string"
		elif var_info["type"] == "time split":
			var_type["type"] = "string"
		else:
			var_type["type"] = var_info["type"]
		# get element type
		if var_type["type"] == "string":
			# string
			if "length" in var_info:
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
		if var_type["type"] != "object":
			del var_options["collapsed"]
		if len(var_options):
			var_type["options"] = var_options
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
			var_type["maxItems"] = var_info["array"]
			if not "empty" in var_info or not var_info["empty"]:
				var_type["minItems"] = var_info["array"]
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
	# open and parse
	with open(infile, "rb") as f:
		data = json.loads(f.read().decode('utf8'), object_pairs_hook=OrderedDict)
		structures = data["structures"]
		config = {}
		config["title"] = " "
		config["format"] = "tabs"
		config["options"] = {"disable_collapse": True}
		if "options" in data:
			if "schema" in data["options"]:
				config.update(data["options"]["schema"])
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

