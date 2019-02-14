#!/usr/bin/env ruby
require 'fileutils'
require 'date'
require 'json'

puts "Runing conversion script"

glob = Dir.glob('data/*')
sumFolder = glob.length
date = DateTime.now.strftime('%s')
manifest = {}
folderCount = 0
outputFolder = "output-#{date}"
total_size = 0

if glob.length <= 10
 glob.sort.each do |folder|
   # do something with the file here
   if(File.directory?(folder))
     foldername = File.basename(folder)
     manifest[foldername] = {}
     # puts foldername
     path = "#{outputFolder}/#{folderCount}"
     folderCount +=1
     FileUtils.mkdir_p path
     i = 0
     Dir.glob("#{folder}/**/*.mp3").sort.each do |filename|
       basename = File.basename(filename)
       outName = "#{i}.mp3"
       manifest[foldername][filename] = "#{path}/#{outName}"
        FileUtils.cp(filename, "#{path}/#{outName}")
        total_size+=File.size(filename)
       i+=1
     end

   end
 end
end

MEGABYTE = 1024.0 * 1024.0
def bytesToMeg bytes
  bytes /  MEGABYTE
end

# puts total_size.to_s + ' bytes'  # displays 62651176 bytes
puts bytesToMeg(total_size).to_s + ' MB'  # displays 59.7488174438477 MB

File.open("#{outputFolder}/manifest.json","w") do |f|
  f.write(manifest.to_json)
end
