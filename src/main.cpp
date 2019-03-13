/**
 * @file main.cpp
 *
 * This module holds the main() function, which is the entrypoint
 * to the program.
 *
 * © 2019 by Richard Walters
 */

#include <fstream>
#include <functional>
#include <Hash/Sha2.hpp>
#include <inttypes.h>
#include <MessageHeaders/MessageHeaders.hpp>
#include <Sasl/Client/Login.hpp>
#include <Sasl/Client/Plain.hpp>
#include <Sasl/Client/Scram.hpp>
#include <Smtp/Client.hpp>
#include <SmtpAuth/Client.hpp>
#include <SystemAbstractions/DiagnosticsStreamReporter.hpp>
#include <signal.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

    /**
     * This function prints to the standard error stream information
     * about how to use this program.
     */
    void PrintUsageInformation() {
        fprintf(
            stderr,
            (
                "Usage: Newman MAIL CERTS\n"
                "\n"
                "Send an e-mail using SMTP.\n"
                "\n"
                "MAIL   Path to file (in Electronic Mail Format, or .eml) containing\n"
                        "the e-mail to send.  The e-mail should contain custom headers\n"
                        "(X-SMTP-Hostname, X-SMTP-Port, X-SMTP-Username, X_SMTP-Password)\n"
                        "which are stripped out before sending, and used to configure\n"
                        "the SMTP client.\n"
                "\n"
                "CERTS  Path to file (in Privacy Enhanced Mail format, or .pem)\n"
                        "containing one or more SSL certificates which the client should\n"
                        "consider trusted and root certificate authorites.\n"
            )
        );
    }

    /**
     * This flag indicates whether or not the application should shut down.
     */
    bool shutDown = false;

    /**
     * This contains variables set through the operating system environment
     * or the command-line arguments.
     */
    struct Environment {
        /**
         * This is the path to the file containing the e-mail.
         */
        std::string emailFileName;

        /**
         * This is the path to the file containing the CA certificates.
         */
        std::string caCertsFileName;
    };

    /**
     * This function is set up to be called when the SIGINT signal is
     * received by the program.  It just sets the "shutDown" flag
     * and relies on the program to be polling the flag to detect
     * when it's been set.
     *
     * @param[in] sig
     *     This is the signal for which this function was called.
     */
    void InterruptHandler(int) {
        shutDown = true;
    }

    /**
     * This function updates the program environment to incorporate
     * any applicable command-line arguments.
     *
     * @param[in] argc
     *     This is the number of command-line arguments given to the program.
     *
     * @param[in] argv
     *     This is the array of command-line arguments given to the program.
     *
     * @param[in,out] environment
     *     This is the environment to update.
     *
     * @param[in] diagnosticMessageDelegate
     *     This is the function to call to publish any diagnostic messages.
     *
     * @return
     *     An indication of whether or not the function succeeded is returned.
     */
    bool ProcessCommandLineArguments(
        int argc,
        char* argv[],
        Environment& environment,
        SystemAbstractions::DiagnosticsSender::DiagnosticMessageDelegate diagnosticMessageDelegate
    ) {
        bool emailFileNameSet = false;
        size_t state = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            switch (state) {
                case 0: { // MAIL
                    environment.emailFileName = arg;
                    emailFileNameSet = true;
                    state = 1;
                } break;

                case 1: { // CERTS (optional)
                    environment.caCertsFileName = arg;
                    state = 2;
                } break;

                case 2: { // extra argument
                    diagnosticMessageDelegate(
                        "Newman",
                        SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                        "extra arguments given"
                    );
                    return false;
                } break;
            }
        }
        if (!emailFileNameSet) {
            diagnosticMessageDelegate(
                "Newman",
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "no MAIL given"
            );
            return false;
        }
        return true;
    }

    using LoginFunction = std::function<
        void(
            const std::string& username,
            const std::string& password
        )
    >;

    LoginFunction SetupClient(
        Smtp::Client& client,
        const std::string& caCertsFileName
    ) {
        auto saslLogin = std::make_shared< Sasl::Client::Login >();
        auto auth = std::make_shared< SmtpAuth::Client >();
        auth->Configure("LOGIN", saslLogin);
        client.RegisterExtension("AUTH", auth);
        std::ifstream caCertsFile(caCertsFileName);
        std::ostringstream caCertsBuilder;
        std::string line;
        while (std::getline(caCertsFile, line)) {
            caCertsBuilder << line << "\r\n";
        }
        client.EnableTls(caCertsBuilder.str());
        return [saslLogin](
            const std::string& username,
            const std::string& password
        ){
            saslLogin->SetCredentials(password, username);
        };
    }

    struct Email {
        MessageHeaders::MessageHeaders headers;
        std::string body;
    };

    Email ReadEmail(const std::string& emailFileName) {
        Email email;
        std::ifstream emailFile(emailFileName);
        std::string buffer;
        std::string line;
        bool headersComplete = false;
        while (std::getline(emailFile, line)) {
            const std::string lineWithNewLine = line + "\r\n";
            if (headersComplete) {
                email.body += lineWithNewLine;
            } else {
                buffer += lineWithNewLine;
                size_t bytesConsumed = 0;
                const auto headersParseResponse = email.headers.ParseRawMessage(
                    buffer,
                    bytesConsumed
                );
                if (bytesConsumed == buffer.length()) {
                    buffer.clear();
                } else {
                    buffer = buffer.substr(bytesConsumed);
                }
                if (headersParseResponse == MessageHeaders::MessageHeaders::State::Complete) {
                    headersComplete = true;
                    email.body = buffer;
                }
            }
        }
        return email;
    }

}

/**
 * This function is the entrypoint of the program.
 * It just sets up the web client, using it to fetch a resource
 * and generate a report.  It registers the SIGINT signal to know
 * when the web client should be shut down early.
 *
 * The program is terminated after either a report is generated
 * or the SIGINT signal is caught.
 *
 * @param[in] argc
 *     This is the number of command-line arguments given to the program.
 *
 * @param[in] argv
 *     This is the array of command-line arguments given to the program.
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    //_crtBreakAlloc = 18;
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif /* _WIN32 */
    const auto previousInterruptHandler = signal(SIGINT, InterruptHandler);
    Environment environment;
    (void)setbuf(stdout, NULL);
    const auto diagnosticsPublisher = SystemAbstractions::DiagnosticsStreamReporter(stdout, stderr);
    if (!ProcessCommandLineArguments(argc, argv, environment, diagnosticsPublisher)) {
        PrintUsageInformation();
        return EXIT_FAILURE;
    }
    Smtp::Client client;
    const auto provideCredentials = SetupClient(client, environment.caCertsFileName);
    auto email = ReadEmail(environment.emailFileName);
    diagnosticsPublisher("Newman", 3, "Connecting to SMTP server.");
    const auto clientHostName = email.headers.GetHeaderValue("X-SMTP-Client-Hostname");
    email.headers.RemoveHeader("X-SMTP-Client-Hostname");
    const auto serverHostName = email.headers.GetHeaderValue("X-SMTP-Server-Hostname");
    email.headers.RemoveHeader("X-SMTP-Server-Hostname");
    const auto serverPortNumberAsString = email.headers.GetHeaderValue("X-SMTP-Port");
    email.headers.RemoveHeader("X-SMTP-Port");
    const auto username = email.headers.GetHeaderValue("X-SMTP-Username");
    email.headers.RemoveHeader("X-SMTP-Username");
    const auto password = email.headers.GetHeaderValue("X-SMTP-Password");
    email.headers.RemoveHeader("X-SMTP-Password");
    provideCredentials(username, password);
    uint16_t serverPortNumber = 0;
    (void)sscanf(
        serverPortNumberAsString.c_str(),
        "%" SCNu16,
        &serverPortNumber
    );
    auto futureConnectSuccess = client.Connect(
        clientHostName,
        serverHostName,
        serverPortNumber
    );
    const auto connectSuccess = futureConnectSuccess.get();
    if (connectSuccess) {
        diagnosticsPublisher("Newman", 3, "Connected to SMTP server.");
    } else {
        diagnosticsPublisher(
            "Newman",
            SystemAbstractions::DiagnosticsSender::Levels::WARNING,
            "There was a problem connecting to the SMTP server!"
        );
        return EXIT_FAILURE;
    }
    diagnosticsPublisher("Newman", 3, "Preparing to send e-mail...");
    auto readyToSend = client.GetMessageReadyBeSentFuture();
    auto failure = client.GetFailureFuture();
    if (
        readyToSend.wait_for(std::chrono::milliseconds(5000))
        != std::future_status::ready
    ) {
        if (
            failure.wait_for(std::chrono::seconds(0))
            == std::future_status::ready
        ) {
            diagnosticsPublisher(
                "Newman",
                SystemAbstractions::DiagnosticsSender::Levels::WARNING,
                "There was a problem setting up to send the e-mail!"
            );
            return EXIT_FAILURE;
        } else {
            diagnosticsPublisher(
                "Newman",
                SystemAbstractions::DiagnosticsSender::Levels::WARNING,
                "Timeout waiting to set up to send the e-mail!"
            );
            return EXIT_FAILURE;
        }
    }
    diagnosticsPublisher("Newman", 3, "Sending e-mail.");
    auto futureSendSuccess = client.SendMail(email.headers, email.body);
    diagnosticsPublisher("Newman", 3, "Waiting for e-mail to be sent...");
    const auto sendSuccess = futureSendSuccess.get();
    if (sendSuccess) {
        diagnosticsPublisher("Newman", 3, "E-mail successfully sent.");
    } else {
        diagnosticsPublisher(
            "Newman",
            SystemAbstractions::DiagnosticsSender::Levels::WARNING,
            "There was a problem sending the e-mail!"
        );
        return EXIT_FAILURE;
    }
//    const auto diagnosticsSubscription = client.SubscribeToDiagnostics(diagnosticsPublisher);
    (void)signal(SIGINT, previousInterruptHandler);
    diagnosticsPublisher("Newman", 3, "Exiting...");
    return EXIT_SUCCESS;
}
