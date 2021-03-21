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
type_c_struct = [False, 0, "KS_TYPEDEF_STRUCT", "kgstruct_object_t"]
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

def recursive_struct_dump(output, struct_name, base_offs_str):
	struct = struct_list[struct_name]["template"]
	struct_idx = struct_list[struct_name]["idx"]
	for var_name in struct:
		var_info = struct[var_name]
		type_def = var_info["type_def"]
		offs_str = base_offs_str + "offsetof(%s_t,%s)" % (struct_name, var_name)
		if "key" in var_info:
			var_name = var_info["key"]
		if "struct" in type_def:
			# structure magic
			output.write("\t{\"%s\", (const kgstruct_type_t*)&__kst_%d, %d, %d},\n" % (var_name, var_info["type_gen"], type_def["struct"][0], struct_idx))
			# return magic
			output.write("\t{NULL, (const kgstruct_type_t*)&__kst_%d, %s, %s},\n" % (var_info["type_gen"], struct_idx, type_def["struct"][0]))
			# recursive dump
			recursive_struct_dump(output, type_def["struct"][1], offs_str + " + ")
		else:
			# normal variable
			output.write("\t{\"%s\", (const kgstruct_type_t*)&__kst_%d, %s, %d},\n" % (var_name, var_info["type_gen"], offs_str, struct_idx))

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
idx = 0
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
			type_def = {"type": type_c_string[0], "size": var_info["size"], "array": var_info["size"]}
			type_gen = {"kstype": type_c_string[2], "extra": [var_info["size"]-1], "template": type_c_string[3]}
			type_gen_idx = add_type(type_gen)
		else:
			# get type from info
			if not type_info in type_info_list:
				# extra check
				if type_info in struct_list:
					# struct-in-struct
					dest = [struct_list[type_info]["idx"], type_info]
					type_def = {"type": type_info + "_t", "size": struct_list[type_info]["size"], "struct": dest}
					type_gen = {"kstype": type_c_struct[2], "extra": [struct_list[type_info]["size"]], "template": type_c_struct[3]}
					type_gen_idx = add_type(type_gen)
				else:
					# invalid type
					raise Exception("Unknown type name '%s' for '%s' in '%s'." % (type_info, var_name, struct_name))
			else:
				# built-in value
				type_info = type_info_list[type_info]
				type_def = {"type": type_info[0], "size": type_info[1]}
				type_gen = {"kstype": type_info[2], "extra": [], "template": template_base}
				# check for limits
				if "min" in var_info:
					type_gen["kstype"] += " | " + type_has_min
					type_gen["extra"].append(var_info["min"])
					type_gen["template"] = type_info[3]
				if "max" in var_info:
					type_gen["kstype"] += " | " + type_has_max
					type_gen["extra"].append(var_info["max"])
					type_gen["template"] = type_info[3]
				# add this type
				type_gen_idx = add_type(type_gen)
		# add info
		var_template["type_def"] = type_def
		var_template["type_gen"] = type_gen_idx
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
	# padding, last variable
	if "autopad" in config:
		pad = config["autopad"]
		mod = offset % pad
		if mod > 0:
			pad -= mod
			print("extra pad", pad)
			offset += pad
	struct_list[struct_name] = {"template": struct_template, "size": offset, "idx": idx}
	idx += 1

# generate H file
output = open("%s.h" % sys.argv[2], "w")
output.write("// KGSTRUCT AUTO GENERATED FILE\n")
# export all structures
for struct_name in struct_list:
	struct = struct_list[struct_name]["template"]
	# structure def
	output.write("typedef struct %s_s\n{\n" % struct_name)
	# export all variables
	idx = 0
	for var_name in struct:
		var_info = struct[var_name]
		type_def = var_info["type_def"]
		if "padding" in var_info:
			output.write("\tuint8_t __pad_%u[%u];\n" % (idx, var_info["padding"]))
			idx += 1
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
	struct = struct_list[struct_name]["template"]
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
for type_gen in export_types:
	output.write("static %s __kst_%d =\n{\n" % (type_gen["template"], idx))
	# kstype
	output.write("\t.base.type = %s,\n" % type_gen["kstype"])
	# extra info
	if len(type_gen["extra"]) > 0:
		output.write("\t.extra = {\n")
		for stuff in type_gen["extra"]:
			output.write("\t\t%s,\n" % stuff)
		output.write("\t},\n")
	output.write("};\n")
	idx += 1
# extra newline
output.write("\n")
# export all templates
for struct_name in struct_list:
	# template def
	output.write("const kgstruct_template_t ks_template__%s[] =\n{\n" % struct_name)
	# export all variables
	recursive_struct_dump(output, struct_name, "")
	output.write("\t{NULL, NULL}\n};\n")
# done
output.close()

