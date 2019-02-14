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
       # puts "--------> #{path}/#{outName}"
       i+=1
     end

   end
 end
end

# puts
puts

File.open("#{outputFolder}/manifest.json","w") do |f|
  f.write(manifest.to_json)
end
