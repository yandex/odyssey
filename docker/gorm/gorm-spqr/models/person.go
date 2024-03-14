package models

type Person struct {
	ID        uint32 `json:"id" gorm:"primary_key"`
	FirstName string `json:"first_name"`
	LastName  string `json:"last_name"`
	Email     string `json:"email"`
}
