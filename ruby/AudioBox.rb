#!/usr/bin/env ruby
require 'fileutils'
require 'date'
require 'json'

puts "Runing conversion script"

glob = Dir.glob('data/*')
date = DateTime.now.strftime('%s')
manifest = {}
files_count_array = Array.new
outputFolder = "output-#{date}"
folder_counter = 0
files_counter = 0
total_size = 0

# we only have 10 buttons on the audiobox so more folders is not allowed.
if glob.length <= 10

  # go into each audio folder
  glob.sort.each do |file_in_audio_folder|

    unless File.file?(file_in_audio_folder)
      file_parent_folder_basename = File.basename(file_in_audio_folder)
      manifest[folder_counter] = {}
      path = "#{outputFolder}/#{folder_counter}"

      FileUtils.mkdir_p path

      files = Dir.glob("#{file_in_audio_folder}/**/*.mp3").sort
      files_count_array[folder_counter] = files.length

      files.each do |filename|
      basename = File.basename(filename)
      outName = "#{files_counter}.mp3"
      manifest[folder_counter][filename] = "#{path}/#{outName}"
      FileUtils.cp(filename, "#{path}/#{outName}")
      total_size+=File.size(filename)
      files_counter +=1
    end

    files_counter = 0
    folder_counter +=1
    end
  end
end

File.open("#{outputFolder}/nfo.txt","w") do |f|
  f.write(files_count_array.join(","))
end

File.open("#{outputFolder}/manifest.json","w") do |f|
  f.write(manifest.to_json)
end

MEGABYTE = 1024.0 * 1024.0
def bytesToMeg bytes
  bytes /  MEGABYTE
end

# puts total_size.to_s + ' bytes'  # displays 62651176 bytes
puts bytesToMeg(total_size).to_s + ' MB'  # displays 59.7488174438477 MB
puts JSON.pretty_generate(manifest)
