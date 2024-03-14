package utils

import "github.com/pg-sharding/gorm-spqr/models"

func DeleteAll() error {
	return models.DB.Migrator().DropTable(&models.Person{})
}
