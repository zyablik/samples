#!/usr/bin/env ruby
require 'prettyprint'
require 'fileutils'
require 'bindata'
require 'pry'
require 'debug'

# mtk image [starts with magic 0x88168858] is somewhat like several files
# glued into one with headers with name and size between them.
# this script extracts that "files" and save and name it according to ones labels

# sample output is somewhat like:
# mtk-img-extractor.rb ~/Aurora-devel-5.0.1.82/md1img-verified.img
# reading partition at offset 0x0 size = 20343780: md1rom
# reading partition at offset 0x1366df0 size = 1753: cert1md
# ...
# reading partition at offset 0x2882c40 size = 7760929: md1_mdmlayout
# reading partition at offset 0x2fe9a70 size = 550: md1_file_map
# writing md1img-verified.img.extracted/00_md1rom
# writing md1img-verified.img.extracted/01_cert1md
# ...
# writing md1img-verified.img.extracted/16_md1_mdmlayout
# writing md1img-verified.img.extracted/17_md1_file_map

abort "usage: #{File.basename($PROGRAM_NAME)} IMAGE_PATH" if ARGV.empty?

class MtkPartition < BinData::Record
  endian :little

  struct :header1 do
    uint32be :magic, assert: -> { abs_offset == 0 ? value == 0x88168858 : true }

    # image file filled with zeroes after partitions
    # if magic is 0 consider it as the beginnig of
    # filler and stop parsing by skip the rest of file
    count_bytes_remaining :bytes_remaining
    skip length: -> { if magic == 0
                        puts "magic1 = 0 at offset 0x#{abs_offset.to_s(16)}, skip #{bytes_remaining} bytes to the end"
                        bytes_remaining
                      else
                        0
                      end
                    }

    uint32 :payload_size
    string :label, read_length: 24
    array :padding, type: :uint8, initial_length: -> {
      puts "reading partition at offset 0x#{magic.abs_offset.to_s(16)} size = #{payload_size.to_s}: #{label}"
      16
    }
  end

  struct :header2 do
    uint32be :magic, assert: -> {abs_offset == 0x30 ? value == 0x89168958 : true}
    uint32 :payload_start
    array :padding, type: :uint8, initial_length: -> { payload_start - parent.header1.num_bytes - magic.num_bytes - payload_start.num_bytes }
  end

  array :data, type: :uint8, initial_length: -> { header1.payload_size }

  # partition offset (from start of the file) in file should be aligned to 16 bytes
  # following line doing absolute align, but looks like BinData implementation details
  # slow it execution to death.
  # skip length: -> { (16 - (data.abs_offset + data.num_bytes) % 16) % 16 }

  # so align only partition size itself, hoping it starts on aligned offset:
  array :align16, type: :uint8, byte_align: 16, initial_length: 0
end

class MtkImage < BinData::Record
  array :partitions, type: :MtkPartition, :read_until => :eof # initial_length: 18
end

image_file = File.open(ARGV[0])

#BinData::trace_reading do
  image  = MtkImage.read(image_file)
#end


#   puts "image.partitions.size = #{image.partitions.size}"
#   image.partitions.each_with_index { |part, i|
#     puts %{
# #{i} ----------------------
# header1 {
#   magic: 0x#{part.header1.magic.to_hex}
#   payload_size: #{part.header1.payload_size} (0x#{"%x" % part.header1.payload_size})
#   label: #{part.header1.label}
# }
#
# header2 {
#   magic: 0x#{part.header2.magic.to_hex}
#   payload_start: 0x#{"%x" % part.header2.payload_start}
# }
#
# data: {
#   size = 0x#{part.data.size.to_s(16)}
#   #{"%02x %02x %02x %02x %02x %02x %02x %02x ... " % part.data.first(8)} #{"%02x %02x %02x %02x %02x %02x %02x %02x" % part.data.last(8)}
# }
#     }
#   }

extracted_dir = File.basename(image_file.path) + ".extracted"
FileUtils.mkdir_p(extracted_dir)
image.partitions.each_with_index { |partition, i|
  part_file = "#{extracted_dir}/#{"%02d" % i}_#{partition.header1.label.gsub("\u0000", '')}"
  puts "writing #{part_file}"
  File.open(part_file, 'wb') {|file| partition.data.write(file)}
}
