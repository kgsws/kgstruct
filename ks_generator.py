#!/usr/bin/python3
import sys
import json
from collections import OrderedDict

comment_offsets = True

type_info_list = {
	"u8": ["uint8_t", 1, "KS_TYPEDEF_U8", "kgstruct_u8_t"],
	"u16": ["uint16_t", 2, "KS_TYPEDEF_U16", "kgstruct_u16_t"],
	"u32": ["uint32_t", 4, "KS_TYPEDEF_U32", "kgstruct_u32_t"],
	"u64": ["uint64_t", 8, "KS_TYPEDEF_U64", "kgstruct_u64_t"],
	"s8": ["int8_t", 1, "KS_TYPEDEF_S8", "kgstruct_s8_t"],
	"s16": ["int16_t", 2, "KS_TYPEDEF_S16", "kgstruct_s16_t"],
	"s32": ["int32_t", 4, "KS_TYPEDEF_S32", "kgstruct_s32_t"],
	"s64": ["int64_t", 8, "KS_TYPEDEF_S64", "kgstruct_s64_t"],
	"f32": ["float", 4, "KS_TYPEDEF_FLOAT", "kgstruct_float_t"],
	"f64": ["double", 8, "KS_TYPEDEF_DOUBLE", "kgstruct_double_t"]
}
type_c_string = ["uint8_t", 0, "KS_TYPEDEF_STRING", "kgstruct_string_t"]
type_has_min = "KS_TYPEFLAG_HAS_MIN"
type_has_max = "KS_TYPEFLAG_HAS_MAX"
template_base = "kgstruct_base_base_t"
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
# MAIN

# args
if len(sys.argv) < 3:
	print("usage:", sys.argv[0], "template.json file_base")
	exit(1)

# open and parse
with open(sys.argv[1], "rb") as f:
	data = json.loads(f.read().decode('utf8'), object_pairs_hook=OrderedDict)
	if "options" in data:
		config = data["options"]
	else:
		config = {}
	structures = data["structures"]

# parse all structures
struct_list = OrderedDict()
for struct_name in structures:
	struct = structures[struct_name]
	struct_template = OrderedDict()
	offset = 0
	for var_name in struct:
		var_info = struct[var_name]
		var_template = {}
		type_info = var_info["type"]
		if type_info == "string":
			# simple C string
			type_def = {"type": type_c_string[0], "size": var_info["size"], "array": var_info["size"], "kstype": type_c_string[2], "extra": [var_info["size"]-1], "template": type_c_string[3]}
			var_template["type"] = add_type(type_def)
		else:
			# get type from info
			if not type_info in type_info_list:
				raise Exception("Unknown type name '%s' for '%s' in '%s'." % (type_info, var_name, struct_name))
			# built-in value
			type_info = type_info_list[type_info]
			type_def = {"type": type_info[0], "size": type_info[1], "kstype": type_info[2], "extra": [], "template": template_base}
			# check for limits
			if "min" in var_info:
				type_def["kstype"] += " | " + type_has_min
				type_def["extra"].append(var_info["min"])
				type_def["template"] = type_info[3]
			if "max" in var_info:
				type_def["kstype"] += " | " + type_has_max
				type_def["extra"].append(var_info["max"])
				type_def["template"] = type_info[3]
			# add this type
			var_template["type"] = add_type(type_def)
		# padding
		if "autopad" in config:
			# make sure this type has correct position
			pad = config["autopad"]
			if pad > type_def["size"]:
				# but never cross specified "autopad" size
				pad = type_def["size"]
			mod = offset % pad
			if mod > 0:
				pad -= mod
				offset += pad
				var_template["padding"] = pad
		# offset
		var_template["offset"] = offset
		offset += type_def["size"]
		# name
		if "key" in var_info:
			var_template["key"] = var_info["key"]
		struct_template[var_name] = var_template
	struct_list[struct_name] = struct_template

# generate H file
output = open("%s.h" % sys.argv[2], "w")
output.write("// KGSTRUCT AUTO GENERATED FILE\n")
# export all structures
for struct_name in struct_list:
	struct = struct_list[struct_name]
	# structure def
	output.write("typedef struct %s_s\n{\n" % struct_name)
	# export all variables
	pad_idx = 0
	for var_name in struct:
		var_info = struct[var_name]
		type_def = export_types[var_info["type"]]
		if "padding" in var_info:
			output.write("\tuint8_t __pad_%u[%u];\n" % (pad_idx, var_info["padding"]))
			pad_idx += 1
		output.write("\t%s %s" % (type_def["type"], var_name))
		if "array" in type_def:
			output.write("[%u]" % type_def["array"])
		if comment_offsets:
			output.write("; // %u\n" % var_info["offset"])
		else:
			output.write(";\n")
	# structure ending
	output.write("} __attribute__((packed)) %s_t;\n" % struct_name)
# extra newline
output.write("\n")
# export all template defs
for struct_name in struct_list:
	struct = struct_list[struct_name]
	# template def
	output.write("extern const kgstruct_template_t ks_template__%s[];\n" % struct_name)
# done
output.close()

# generate C file
output = open("%s.c" % sys.argv[2], "w")
output.write("// KGSTRUCT AUTO GENERATED FILE\n")
output.write("#include <stdint.h>\n#include <stddef.h>\n#include \"kgstruct.h\"\n#include \"%s.h\"\n\n" % sys.argv[2])
# export all types
idx = 0
for type_def in export_types:
	output.write("static %s __kst_%d =\n{\n" % (type_def["template"], idx))
	# kstype
	output.write("\t.base.type = %s,\n" % type_def["kstype"])
	# extra info
	if len(type_def["extra"]) > 0:
		output.write("\t.extra = {\n")
		for stuff in type_def["extra"]:
			output.write("\t\t%s,\n" % stuff)
		output.write("\t},\n")
	output.write("};\n")
	idx += 1
# extra newline
output.write("\n")
# export all templates
for struct_name in struct_list:
	struct = struct_list[struct_name]
	# template def
	output.write("const kgstruct_template_t ks_template__%s[] =\n{\n" % struct_name)
	# export all variables
	for var_name in struct:
		var_info = struct[var_name]
		offs_str = "offsetof(%s_t,%s)" % (struct_name, var_name)
		if "key" in var_info:
			var_name = var_info["key"]
		output.write("\t{\"%s\", (const kgstruct_type_t*)&__kst_%d, %s},\n" % (var_name, var_info["type"], offs_str))
	output.write("\t{NULL}\n};\n")
# done
output.close()

