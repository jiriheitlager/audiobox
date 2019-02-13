 #!/usr/bin/env ruby
require 'fileutils'
require 'Date'

puts "Runing conversion script"

glob = Dir.glob('data/*')
sumFolder = glob.length


# FileUtils.rm_rf("output")


if glob.length <= 10
  folderCount = 0
  glob.each do |folder|
    # do something with the file here

    date = DateTime.now.strftime('%s')

    if(File.directory?(folder))
      foldername = File.basename(folder)
      # puts foldername
      path = "output-#{date}/#{folderCount}"
      folderCount +=1
      FileUtils.mkdir_p path
      i = 0
      Dir.glob("#{folder}/**/*.mp3") do |filename|
        basename = File.basename(filename)
        FileUtils.cp(filename, "#{path}/#{i}.mp3")
        i+=1
      end

    end
  end
end
