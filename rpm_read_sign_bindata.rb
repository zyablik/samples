#!/usr/bin/ruby
require 'prettyprint'
require 'bindata'

# http://ftp.rpm.org/api/4.4.2.2/pkgformat.html
# https://github.com/dmendel/bindata

class RpmLead < BinData::Record
  endian :big

  array :magic, type: :uint8, initial_length: 4
  uint8 :major
  uint8 :minor
  int16 :ttype
  int16 :archnum
  string :name, read_length: 66
  int16 :osnum
  int16 :signature_type
  array :reserved, type: :uint8, initial_length: 16
end

class IndexEntry < BinData::Record
  endian :big

  int32 :tag
  int32 :ttype
  int32 :offset
  int32 :ccount
end

class RpmHeader < BinData::Record
  endian :big

  array :magic, type: :uint8, initial_length: 3
  uint8 :version
  array :reserved, type: :uint8, initial_length: 4

  int32 :entries_count
  int32 :entries_data_size

  array :iindex, type: :index_entry, initial_length: :entries_count

  array :data, type: :uint8, initial_length: -> { entries_data_size }
end


io = File.open("./rpm/RPMS/armv7hl/hello-1.0-1.armv7hl.rpm")

#BinData::trace_reading do
  lead  = RpmLead.read(io)
  pp lead
  puts "lead magic = %x %x %x %x" % lead.magic

  puts "lead io.pos = %d (0x%x)" % [io.pos, io.pos]

#end

  signature  = RpmHeader.read(io)
  pp signature
  puts "signature magic = %x %x %x" % signature.magic
  puts "signature data = #{signature.data.size}"

  puts "signature io.pos = %d (0x%x)" % [io.pos, io.pos]

  aligned_pos = (io.pos / 8.0).ceil * 8;
  puts "io.pos round up = #{aligned_pos}"

  BinData::Int32be.read(io) # what is it?

  header  = RpmHeader.read(io)
  pp header
  puts "header magic = %x %x %x" % header.magic
  puts "header data = #{header.data.size}"

  puts "header io.pos = %d (0x%x)" % [io.pos, io.pos]

class GzipHeader < BinData::Record
  endian :big

  array :magic, type: :uint8, initial_length: 2
  uint8 :mmethod
end

gzip_header  = GzipHeader.read(io)

puts "gzip header: magic %x %x method: %x" % [gzip_header.magic, gzip_header.mmethod].flatten
