unset exit

set link false
set rezfork false

clearmem

for {header} in vncview vncsession vncdisplay colortables menus \
                desktopsize mouse keyboard copyrect raw hextile clipboard
        unset exit
        newer VNCview.GS {header}.h
        if {status} != 0
                set exit on
                delete -P -W =.a
        end
end

for file in vncview vncsession vncdisplay colortables \
            desktopsize mouse keyboard copyrect raw hextile clipboard
        unset exit
        newer {file}.a {file}.cc
        if {status} != 0
                set exit on
                compile +O {file}.cc keep={file}
                set link true
        end
end

unset exit
newer vncview.rezfork vncview.rez
if {status} != 0
        set exit on
        compile vncview.rez keep=vncview.rezfork
        copy -C -P -R vncview.rezfork VNCview.GS
end

if {link} == true
        link vncview vncsession vncdisplay colortables \
             desktopsize mouse keyboard copyrect raw hextile clipboard \
             keep=VNCview.GS
        filetype VNCview.GS S16 $DB03
end
