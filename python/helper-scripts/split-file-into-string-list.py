import argparse


def chunk_lines(lines, max_chars):
    chunks = []
    current_chunk = []
    current_length = 0

    for line in lines:
        line_length = len(line)

        # If a single line exceeds max_chars, put it in its own chunk
        if line_length > max_chars:
            if current_chunk:
                chunks.append("".join(current_chunk))
                current_chunk = []
                current_length = 0
            chunks.append(line)
            continue

        if current_length + line_length > max_chars:
            chunks.append("".join(current_chunk))
            current_chunk = [line]
            current_length = line_length
        else:
            current_chunk.append(line)
            current_length += line_length

    if current_chunk:
        chunks.append("".join(current_chunk))

    return chunks


def safe_format(template, index):
    """
    Format template with index if it contains a format placeholder.
    Otherwise return it unchanged.
    """
    try:
        return template.format(index)
    except (IndexError, KeyError, ValueError):
        return template


def process_file(input_path, max_chars, prefix, suffix, output_path=None):
    with open(input_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    chunks = chunk_lines(lines, max_chars)

    wrapped_chunks = []
    for i, chunk in enumerate(chunks):
        formatted_prefix = safe_format(prefix, i)
        formatted_suffix = safe_format(suffix, i)
        wrapped_chunks.append(f"{formatted_prefix}{chunk}{formatted_suffix}")

    if output_path:
        with open(output_path, "w", encoding="utf-8") as f:
            for chunk in wrapped_chunks:
                f.write(chunk + "\n")
    else:
        for chunk in wrapped_chunks:
            print(chunk)
            print("-" * 40)


def main():
    parser = argparse.ArgumentParser(description="Chunk file lines by character limit.")
    parser.add_argument("input", help="Input file path")
    parser.add_argument("--max-chars", type=int, required=True, help="Max characters per chunk")
    parser.add_argument("--prefix", default="", help="String to prepend to each chunk (supports {0})")
    parser.add_argument("--suffix", default="", help="String to append to each chunk (supports {0})")
    parser.add_argument("--output", help="Optional output file path")

    args = parser.parse_args()

    process_file(
        input_path=args.input,
        max_chars=args.max_chars,
        prefix=args.prefix,
        suffix=args.suffix,
        output_path=args.output
    )


if __name__ == "__main__":
    main()
