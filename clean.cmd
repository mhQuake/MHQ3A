attrib *.ncb -r -a -s -h /s /d
attrib *.suo -r -a -s -h /s /d
attrib *.user -r -a -s -h /s /d

del *.ncb /s /q
del *.sdf /s /q
del *.suo /s /q
del *.user /s /q

rmdir ipch /s /q
rmdir Debug /s /q
rmdir Release /s /q

cd Q3A

rmdir Debug /s /q
rmdir Release /s /q

cd ..

echo done
pause
