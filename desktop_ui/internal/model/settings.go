package model

type AppSettings struct {
	StorageFile string `json:"storage_file"`
}

func DefaultSettings() AppSettings {
	return AppSettings{
		StorageFile: "lora_storage.json",
	}
}
