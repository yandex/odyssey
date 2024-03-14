package tests

import (
	"fmt"
	"os"
	"testing"

	"github.com/pg-sharding/gorm-spqr/controllers"
	"github.com/pg-sharding/gorm-spqr/models"
	"github.com/pg-sharding/gorm-spqr/utils"
	"gotest.tools/v3/assert"
	is "gotest.tools/v3/assert/cmp"
)

func setup() {
	models.ConnectDatabase()
}

func tearDown(t *testing.T) {
	if err := utils.DeleteAll(); err != nil {
		t.Errorf("Error running tearDown: %s", err)
	}
}

func TestMain(m *testing.M) {
	models.SetupSharding()
	code := m.Run()
	_ = utils.DeleteAll()
	os.Exit(code)
}

func TestCreatePerson(t *testing.T) {
	setup()

	var person = models.Person{
		ID:        1,
		FirstName: "John",
		LastName:  "Smith",
	}

	err := controllers.WritePerson(&person)
	assert.Check(t, err, "could not write: %s", err)

	var allPersons = controllers.GetAllPersons()
	assert.Check(t, is.Len(allPersons, 1), "Expected to have 1 person in db, got %d", len(allPersons))

	tearDown(t)
}

func TestReadPerson(t *testing.T) {
	setup()

	var person = models.Person{
		ID:        1,
		FirstName: "John",
		LastName:  "Smith",
	}

	err := controllers.WritePerson(&person)
	assert.Check(t, err, "could not write: %s", err)

	var personDb *models.Person
	personDb, err = controllers.GetPerson(person.ID)
	assert.Check(t, err, "error getting person: %s", err)
	assert.DeepEqual(t, person, *personDb)

	tearDown(t)
}

func TestUpdatePerson(t *testing.T) {
	setup()

	var person = models.Person{
		ID:        1,
		FirstName: "John",
		LastName:  "Smith",
	}

	err := controllers.WritePerson(&person)
	assert.Check(t, err, "could not write: %s", err)

	person.Email = "foo@bar"

	err = controllers.UpdatePerson(&person)
	assert.Check(t, err, "error updating: %s", err)

	var personDb *models.Person
	personDb, err = controllers.GetPerson(person.ID)
	assert.Check(t, err, "error getting person: %s", err)
	assert.DeepEqual(t, person, *personDb)

	tearDown(t)
}

func TestDeletePerson(t *testing.T) {
	setup()

	var person = models.Person{
		ID:        1,
		FirstName: "John",
		LastName:  "Smith",
	}

	err := controllers.WritePerson(&person)
	assert.Check(t, err, "could not write: %s", err)

	var allPersons = controllers.GetAllPersons()
	assert.Check(t, is.Len(allPersons, 1), "Expected to have 1 person in db, got %d", len(allPersons))

	err = controllers.DeletePerson(person.ID)
	assert.Check(t, err, "error deleting person: %s", err)

	allPersons = controllers.GetAllPersons()
	assert.Check(t, is.Len(allPersons, 0), "Expected to have no people in db, got %d", len(allPersons))

	tearDown(t)
}

func TestMultipleWrite(t *testing.T) {
	setup()

	var firstShard = make([]*models.Person, 0)
	for i := 1; i < 100; i++ {
		person := &models.Person{
			ID:        uint32(i),
			FirstName: fmt.Sprintf("Name_%d", i),
		}
		firstShard = append(firstShard, person)
	}

	err := controllers.WritePeople(firstShard)
	assert.Check(t, err, "could not write: %s", err)

	var secondShard = make([]*models.Person, 0)
	for i := 100; i < 200; i++ {
		person := &models.Person{
			ID:        uint32(i),
			FirstName: fmt.Sprintf("Name_%d", i),
		}
		secondShard = append(secondShard, person)
	}

	err = controllers.WritePeople(secondShard)
	assert.Check(t, err, "could not write: %s", err)

	firstShardPeople, err := controllers.GetPeople(uint32(1), uint32(99))
	assert.Check(t, err, "could not get people: %s", err)

	assert.Check(t, is.Equal(len(firstShardPeople), 99), "expected to have %d records on 1st shard, got %d", 99, len(firstShardPeople))

	secondShardPeople, err := controllers.GetPeople(uint32(100), uint32(199))
	assert.Check(t, err, "could not get people: %s", err)

	assert.Check(t, is.Equal(len(secondShardPeople), 100), "expected to have %d records on 1st shard, got %d", 100, len(secondShardPeople))

	tearDown(t)
}
