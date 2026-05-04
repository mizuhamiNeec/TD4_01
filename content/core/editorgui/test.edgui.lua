function Draw()
	Ui_Text("Luaからのご挨拶!")
	Ui_Text("ナイフだぁぁああああああああああああ")
	
	if Ui_Button("いい感じのボタン") then
		Ui_Text("ボタンが押されたよ！")
	end

	for i = 1, 4 do
		Ui_Text("ループの中のテキスト: " .. i)
	end
end
