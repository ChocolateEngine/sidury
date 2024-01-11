#pragma once


struct FilePickerData_t
{
	bool                       open;
	bool                       allowMultiSelect;  // NOT IMPLEMENTED YET
	std::string                path;
	std::vector< std::string > filterExt;
	std::vector< std::string > selectedItems;     // Get the selected item here
	std::vector< std::string > filesInFolder;
};


enum EFilePickerReturn
{
	EFilePickerReturn_None,
	EFilePickerReturn_Close,
	EFilePickerReturn_SelectedItems,
};


EFilePickerReturn FilePicker_Draw( FilePickerData_t& srData, const char* spWindowName );

