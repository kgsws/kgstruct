#!/usr/bin/python3
import sys
import json
from collections import OrderedDict

comment_offsets = True

type_info_list = {
	"u8": ["uint8_t", 1, "KS_TYPEDEF_U8"],
	"u16": ["uint16_t", 2, "KS_TYPEDEF_U16"],
	"u32": ["uint32_t", 4, "KS_TYPEDEF_U32"],
	"u64": ["uint64_t", 8, "KS_TYPEDEF_U64"],
	"s8": ["int8_t", 1, "KS_TYPEDEF_S8"],
	"s16": ["int16_t", 2, "KS_TYPEDEF_S16"],
	"s32": ["int32_t", 4, "KS_TYPEDEF_S32"],
	"s64": ["int64_t", 8, "KS_TYPEDEF_S64"],
	"f32": ["float", 4, "KS_TYPEDEF_FLOAT"],
	"f64": ["double", 8, "KS_TYPEDEF_DOUBLE"]
}
type_c_string = ["uint8_t", 0, "KS_TYPEDEF_STRING"]
type_has_min = "KS_TYPEFLAG_HAS_MIN"
type_has_max = "KS_TYPEFLAG_HAS_MAX"
export_types = []


##
def add_type(tdef):
	global export_types
	i = 0
	for name in export_types:
		if tdef == name:
			print("duplicate type")
			return i
		i += 1
	print("adding new type")
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
	print(struct_name)
	struct = structures[struct_name]
	struct_template = OrderedDict()
	offset = 0
	for var_name in struct:
		print(var_name)
		var_info = struct[var_name]
		var_template = {}
		type_info = var_info["type"]
		if type_info == "string":
			# simple C string
			type_def = {"type": type_c_string[0], "size": var_info["size"], "array": var_info["size"], "extra": [type_c_string[2], var_info["size"]]}
			var_template["type"] = add_type(type_def)
		else:
			# get type from info
			if not type_info in type_info_list:
				raise Exception("Unknown type name '" + type_info + "' for '" + var_name + "' in '" + struct_name + "'.")
			# built-in value
			type_info = type_info_list[type_info]
			type_def = {"type": type_info[0], "size": type_info[1], "extra": [type_info[2]]}
			# check for limits
			if "min" in var_info:
				type_def["extra"][0] += " | " + type_has_min
				type_def["extra"].append(var_info["min"])
			if "max" in var_info:
				type_def["extra"][0] += " | " + type_has_max
				type_def["extra"].append(var_info["max"])
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
output = open(sys.argv[2] + ".h", "w")
output.write("// KGSTRUCT AUTO GENERATED FILE\n")
# export all structures
for struct_name in struct_list:
	struct = struct_list[struct_name]
	# structure def
	output.write("typedef struct " + struct_name + "_s\n{\n")
	# export all variables
	pad_idx = 0
	for var_name in struct:
		var_info = struct[var_name]
		type_def = export_types[var_info["type"]]
		if "padding" in var_info:
			output.write("\tuint8_t __pad_" + str(pad_idx) + "[" + str(var_info["padding"]) + "];\n")
			pad_idx += 1
		output.write("\t" + type_def["type"] + " " + var_name)
		if "array" in type_def:
			output.write("[" + str(type_def["array"]) + "]")
		if comment_offsets:
			output.write("; // " + str(var_info["offset"]) + "\n")
		else:
			output.write(";\n")
	# structure ending
	output.write("} __attribute__((packed)) " + struct_name + "_t;\n")
output.close()

