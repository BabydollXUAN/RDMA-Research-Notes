        } else if (!strcmp(argv[i], "-interdcDelay")) {

            interdc_delay = timeFromNs(atoi(argv[i + 1]));

            cout << "interdc_delay is: " << interdc_delay << "ps" << endl;

            i++;
            