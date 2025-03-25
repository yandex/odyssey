using Npgsql;

namespace NpgsqlOdysseyScram.Console {
    class EntryPoint {
        private static async Task TestScramAuth() {
            // npgsql sends 'eSws' when no channel binding, odyssey must be ready

            const string connectionString = "Pooling=false;Host=localhost;Port=6432;Username=auth_query_user_scram_sha_256;Password=passwd;Database=auth_query_db;Keepalive=5;Include Error Detail=true;SSL Mode=disable;";

            var dataSourceBuilder = new NpgsqlDataSourceBuilder(connectionString);
            await using var dataSource = dataSourceBuilder.Build();

            await using var connection = await dataSource.OpenConnectionAsync();
            using var command = new NpgsqlCommand("select 1+2;", connection);
            await using var reader = await command.ExecuteReaderAsync();

            await reader.ReadAsync();
            if (reader.GetInt32(0) != 3) {
                throw new Exception("Unexpected result");
            }
        }

        public static async Task Main() {
            await TestScramAuth();
        }
    }
}