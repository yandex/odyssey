package controllers

import "github.com/pg-sharding/gorm-spqr/models"

func GetAllPersons() []models.Person {
	var res []models.Person
	models.DB.Find(&res)

	return res
}

func GetPerson(id uint32) (*models.Person, error) {
	var person models.Person
	if err := models.DB.Where("id = ?", id).First(&person).Error; err != nil {
		return nil, err
	}
	return &person, nil
}

func WritePerson(person *models.Person) error {
	tx := models.DB.Create(person)
	return tx.Error
}

func UpdatePerson(person *models.Person) error {
	var current models.Person
	if err := models.DB.Where("id = ?", person.ID).First(&current).Error; err != nil {
		return err
	}
	models.DB.Save(&person)
	return nil
}

func DeletePerson(id uint32) error {
	var person models.Person
	if err := models.DB.Where("id = ?", id).First(&person).Error; err != nil {
		return err
	}

	models.DB.Delete(&person)
	return nil
}

func WritePeople(people []*models.Person) error {
	tx := models.DB.Create(people)
	return tx.Error
}

func GetPeople(from uint32, to uint32) ([]*models.Person, error) {
	people := make([]*models.Person, 0)
	if err := models.DB.Where("id >= ? AND id <= ?", from, to).Find(&people).Error; err != nil {
		return nil, err
	}
	return people, nil
}
