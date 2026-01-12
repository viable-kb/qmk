#!/usr/bin/env python3
# Copyright 2025 Ira Cooper <ira@wakeful.net>
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Compress viable.json to LZMA and generate C header for embedding in firmware.
Also validates fragment schema if present.
"""

import sys
import os
import lzma
import json


class FragmentValidationError(Exception):
    """Error during fragment schema validation."""
    pass


def count_kle_keys_and_encoders(kle_data):
    """
    Parse KLE data and count regular keys and encoder keys.

    Encoder keys have labels[4] == "e" (after KLE reordering).
    Returns (key_count, encoder_count).
    """
    key_count = 0
    encoder_count = 0

    # labelMap for align=4 (default) from kle_serial.py
    label_map = [0, 6, 2, 8, 10, -1, 3, 5, 1, 4, 7, -1]

    def reorder_labels(labels_str, align=4):
        """Reorder labels according to KLE alignment."""
        labels = labels_str.split("\n")
        # Default align=4 (center front)
        lm = [
            [0, 6, 2, 8, 9, 11, 3, 5, 1, 4, 7, 10],
            [1, 7, -1, -1, 9, 11, 4, -1, -1, -1, -1, 10],
            [3, -1, 5, -1, 9, 11, -1, -1, 4, -1, -1, 10],
            [4, -1, -1, -1, 9, 11, -1, -1, -1, -1, -1, 10],
            [0, 6, 2, 8, 10, -1, 3, 5, 1, 4, 7, -1],
            [1, 7, -1, -1, 10, -1, 4, -1, -1, -1, -1, -1],
            [3, -1, 5, -1, 10, -1, -1, -1, 4, -1, -1, -1],
            [4, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1],
        ]
        ret = [None] * 12
        for i, label in enumerate(labels):
            if i < len(lm[align]) and lm[align][i] != -1 and label:
                ret[lm[align][i]] = label
        return ret

    align = 4
    for row in kle_data:
        if not isinstance(row, list):
            continue
        for item in row:
            if isinstance(item, str):
                labels = reorder_labels(item, align)
                # Check if encoder: labels[4] == "e"
                if len(labels) > 4 and labels[4] == "e":
                    encoder_count += 1
                else:
                    key_count += 1
            elif isinstance(item, dict):
                if 'a' in item:
                    align = item['a']

    # Encoders come in pairs (CW and CCW), so actual encoder count is half
    return key_count, encoder_count // 2


def validate_fragment_schema(data):
    """
    Validate fragment schema in 6 phases as specified in the plan.
    Raises FragmentValidationError on first error.
    """
    # Check if fragments are present - if not, skip validation
    if 'fragment_schema_version' not in data and 'fragments' not in data:
        return  # No fragment schema to validate

    # Phase 1: Schema structure
    if 'fragment_schema_version' not in data:
        raise FragmentValidationError("Missing 'fragment_schema_version' field")

    version = data['fragment_schema_version']
    if not isinstance(version, int) or version > 1:
        raise FragmentValidationError(
            f"Unsupported fragment schema version {version} (max supported: 1)"
        )

    if 'fragments' not in data:
        raise FragmentValidationError("Missing 'fragments' section")

    fragments = data['fragments']
    if not isinstance(fragments, dict):
        raise FragmentValidationError("'fragments' must be an object")

    if 'composition' not in data:
        raise FragmentValidationError("Missing 'composition' section")

    composition = data['composition']
    if 'instances' not in composition:
        raise FragmentValidationError("Missing 'composition.instances' field")

    instances = composition['instances']
    if not isinstance(instances, list):
        raise FragmentValidationError("'composition.instances' must be an array")

    # Phase 2: Fragment definitions
    fragment_ids = {}  # id -> fragment_name
    for frag_name, frag in fragments.items():
        if 'id' not in frag:
            raise FragmentValidationError(
                f"Fragment '{frag_name}' is missing required 'id' field"
            )

        frag_id = frag['id']
        if not isinstance(frag_id, int) or frag_id < 0 or frag_id > 254:
            raise FragmentValidationError(
                f"Fragment '{frag_name}' has invalid id {frag_id} (must be 0-254)"
            )

        if frag_id == 255:
            raise FragmentValidationError(
                f"Fragment '{frag_name}' uses reserved id 255 (0xFF)"
            )

        if frag_id in fragment_ids:
            raise FragmentValidationError(
                f"Duplicate fragment id {frag_id} (used by '{fragment_ids[frag_id]}' and '{frag_name}')"
            )

        fragment_ids[frag_id] = frag_name

        if 'kle' not in frag:
            raise FragmentValidationError(
                f"Fragment '{frag_name}' is missing required 'kle' field"
            )

    # Phase 3: Instance structure
    if len(instances) > 21:
        raise FragmentValidationError(
            f"Too many instances: {len(instances)} (max 21)"
        )

    instance_ids = {}  # string id -> position
    for idx, instance in enumerate(instances):
        if 'id' not in instance:
            raise FragmentValidationError(
                f"Instance at position {idx} is missing required 'id' field"
            )

        inst_id = instance['id']
        if not isinstance(inst_id, str):
            raise FragmentValidationError(
                f"Instance at position {idx} has non-string 'id': {inst_id}"
            )

        if inst_id in instance_ids:
            raise FragmentValidationError(
                f"Duplicate instance id '{inst_id}' (at positions {instance_ids[inst_id]} and {idx})"
            )

        instance_ids[inst_id] = idx

        has_fragment = 'fragment' in instance
        has_options = 'fragment_options' in instance

        if not has_fragment and not has_options:
            raise FragmentValidationError(
                f"Instance '{inst_id}' must have either 'fragment' or 'fragment_options'"
            )

        if has_fragment and has_options:
            raise FragmentValidationError(
                f"Instance '{inst_id}' has both 'fragment' and 'fragment_options'; use only one"
            )

    # Phase 4: Fragment options structure
    for idx, instance in enumerate(instances):
        inst_id = instance['id']

        if 'fragment_options' not in instance:
            continue

        options = instance['fragment_options']
        if not isinstance(options, list):
            raise FragmentValidationError(
                f"Instance '{inst_id}' has non-array 'fragment_options'"
            )

        if len(options) < 2:
            raise FragmentValidationError(
                f"Instance '{inst_id}' has fragment_options with only {len(options)} entry; use fixed 'fragment' instead"
            )

        default_count = 0
        for opt_idx, opt in enumerate(options):
            if not isinstance(opt, dict):
                raise FragmentValidationError(
                    f"Instance '{inst_id}' fragment_options[{opt_idx}] is not an object"
                )

            for required in ['fragment', 'placement', 'matrix_map']:
                if required not in opt:
                    raise FragmentValidationError(
                        f"Instance '{inst_id}' fragment_options[{opt_idx}] is missing '{required}'"
                    )

    # Phase 5: Fragment references
    def check_fragment_ref(frag_name, inst_id, context=""):
        if frag_name not in fragments:
            raise FragmentValidationError(
                f"Fragment '{frag_name}' not found (referenced by instance '{inst_id}'{context})"
            )

    for idx, instance in enumerate(instances):
        inst_id = instance['id']

        if 'fragment' in instance:
            check_fragment_ref(instance['fragment'], inst_id)

        if 'fragment_options' in instance:
            for opt_idx, opt in enumerate(instance['fragment_options']):
                check_fragment_ref(opt['fragment'], inst_id, f" option {opt_idx}")

    # Phase 6: KLE parsing and map validation
    # Pre-compute key counts for all fragments
    fragment_key_counts = {}
    for frag_name, frag in fragments.items():
        key_count, encoder_count = count_kle_keys_and_encoders(frag['kle'])
        fragment_key_counts[frag_name] = (key_count, encoder_count)

    def validate_matrix_map(inst_id, frag_name, matrix_map, context=""):
        key_count, _ = fragment_key_counts[frag_name]
        map_len = len(matrix_map)

        if map_len != key_count:
            raise FragmentValidationError(
                f"Instance '{inst_id}'{context} references fragment '{frag_name}' with {key_count} keys but matrix_map has {map_len} entries"
            )

    for idx, instance in enumerate(instances):
        inst_id = instance['id']

        # Validate encoder_offset if present
        if 'encoder_offset' in instance:
            offset = instance['encoder_offset']
            if not isinstance(offset, int) or offset < 0:
                raise FragmentValidationError(
                    f"Instance '{inst_id}' has invalid encoder_offset: {offset} (must be non-negative integer)"
                )

        if 'fragment' in instance:
            if 'matrix_map' not in instance:
                raise FragmentValidationError(
                    f"Instance '{inst_id}' is missing required 'matrix_map'"
                )
            if 'placement' not in instance:
                raise FragmentValidationError(
                    f"Instance '{inst_id}' is missing required 'placement'"
                )
            validate_matrix_map(inst_id, instance['fragment'], instance['matrix_map'])

        if 'fragment_options' in instance:
            for opt_idx, opt in enumerate(instance['fragment_options']):
                # Validate encoder_offset in options if present
                if 'encoder_offset' in opt:
                    offset = opt['encoder_offset']
                    if not isinstance(offset, int) or offset < 0:
                        raise FragmentValidationError(
                            f"Instance '{inst_id}' fragment_options[{opt_idx}] has invalid encoder_offset: {offset}"
                        )

                validate_matrix_map(
                    inst_id, opt['fragment'], opt['matrix_map'],
                    f" fragment_options[{opt_idx}]"
                )


def compress_definition(json_path, output_path):
    """Compress JSON file and generate C header."""

    # Read and minify JSON
    with open(json_path, 'r') as f:
        data = json.load(f)

    # Validate fragment schema if present
    try:
        validate_fragment_schema(data)
    except FragmentValidationError as e:
        print(f"Fragment schema validation error: {e}", file=sys.stderr)
        return False

    # Minify (remove whitespace)
    json_str = json.dumps(data, separators=(',', ':'))
    json_bytes = json_str.encode('utf-8')

    # Compress with LZMA
    compressed = lzma.compress(
        json_bytes,
        format=lzma.FORMAT_ALONE,
        preset=9
    )

    # Generate C header
    with open(output_path, 'w') as f:
        f.write("// Auto-generated by viable_compress.py - DO NOT EDIT\n")
        f.write("// Copyright 2025 Ira Cooper <ira@wakeful.net>\n")
        f.write("// SPDX-License-Identifier: GPL-2.0-or-later\n\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n")
        f.write("#include \"progmem.h\"\n\n")
        f.write(f"#define VIABLE_DEFINITION_SIZE {len(compressed)}\n")
        f.write(f"#define VIABLE_DEFINITION_UNCOMPRESSED_SIZE {len(json_bytes)}\n\n")
        f.write("static const uint8_t PROGMEM viable_definition_data[] = {\n")

        # Write bytes in rows of 16
        for i in range(0, len(compressed), 16):
            chunk = compressed[i:i+16]
            hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"    {hex_bytes},\n")

        f.write("};\n")

    print(f"Compressed {json_path}: {len(json_bytes)} -> {len(compressed)} bytes ({100*len(compressed)//len(json_bytes)}%)")
    return True

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <viable.json> <output.h>")
        sys.exit(1)

    json_path = sys.argv[1]
    output_path = sys.argv[2]

    if not os.path.exists(json_path):
        # No viable.json - generate empty definition
        with open(output_path, 'w') as f:
            f.write("// Auto-generated - no viable.json found\n")
            f.write("#pragma once\n\n")
            f.write("#include <stdint.h>\n")
            f.write("#include \"progmem.h\"\n\n")
            f.write("#define VIABLE_DEFINITION_SIZE 0\n")
            f.write("#define VIABLE_DEFINITION_UNCOMPRESSED_SIZE 0\n\n")
            f.write("static const uint8_t PROGMEM viable_definition_data[] = {};\n")
        print(f"No viable.json found at {json_path}, generated empty definition")
        sys.exit(0)

    if not compress_definition(json_path, output_path):
        sys.exit(1)

if __name__ == '__main__':
    main()
