// See https://aka.ms/new-console-template for more information
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Configuration;
using C2SIM;

// Create and run the C2SIM console service
// For an overview of the dependency injection, logging and configuration enacted
// behind the scenes, see for example:
// https://docs.microsoft.com/en-us/dotnet/core/extensions/generic-host 
// https://snede.net/get-started-with-net-generic-host/ is a simple intro
// but the mechanics it implements are supported more directly by
// BackgroundService - see https://docs.microsoft.com/en-us/dotnet/architecture/microservices/multi-container-microservice-net-applications/background-tasks-with-ihostedservice
// A few different usage patterns are shown here: https://docs.microsoft.com/en-us/aspnet/core/fundamentals/host/hosted-services?view=aspnetcore-3.1&tabs=netcore-cli
await CreateHostBuilder(args).RunConsoleAsync();// .Build.RunAsync();
/// <summary>
// Create the main console service, passing in Logger and C2SIM SDK object configured
// according to appsettings.json parameters, which can be overwritten by command line arguments
/// </summary>
static IHostBuilder CreateHostBuilder(string[] args) =>
    Host.CreateDefaultBuilder(args)
        .ConfigureServices((hostContext, services) =>
        {
            services
                .AddHostedService<C2SIMConsole>();
            services.AddOptions<C2SIMConsoleSettings>()
                .Bind(hostContext.Configuration.GetSection("C2SIM"));
        }); 
