NAME		:= webserv

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98
CPPFLAGS	:= -Iinclude -MMD -MP

SRC_DIR		:= src
OBJ_DIR		:= obj

CONFIG_SRCS	:= config/BuildContext.cpp \
			   config/ConfigBuilder.cpp \
			   config/ConfigError.cpp \
			   config/ConfigLexer.cpp \
			   config/ConfigLoader.cpp \
			   config/ConfigSyntax.cpp \
			   config/ConfigValues.cpp \
			   config/DirectiveRules.cpp \
			   config/LocationBuilder.cpp \
			   config/ServerBuilder.cpp \
			   config/SharedSettings.cpp

CORE_SRCS	:= core/Clock.cpp \
			   core/HttpDate.cpp \
			   core/EventLoop.cpp \
			   core/Listener.cpp \
			   core/Server.cpp \
			   core/Signals.cpp \
			   core/SocketAddress.cpp

ROUTE_SRCS	:= route/PathResolver.cpp \
			   route/Router.cpp

HTTP_SRCS	:= http/Connection.cpp \
			   http/HeaderMap.cpp \
			   http/MimeTypes.cpp \
			   http/Methods.cpp \
			   http/RequestParser.cpp \
			   http/ResponseStream.cpp \
			   http/StatusText.cpp

FS_SRCS		:= fs/FileInfo.cpp \
			   fs/FileReader.cpp

HANDLER_SRCS	:= handler/AutoindexHandler.cpp \
			   handler/ErrorPageBuilder.cpp \
			   handler/StaticHandler.cpp

APP_SRCS	:= app/RequestProcessor.cpp

LIB_SRCS	:= $(CONFIG_SRCS) $(CORE_SRCS) $(FS_SRCS) $(HTTP_SRCS) \
			   $(ROUTE_SRCS) $(HANDLER_SRCS) $(APP_SRCS)
SRCS		:= main.cpp $(LIB_SRCS)

OBJS		:= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))
LIB_OBJS	:= $(addprefix $(OBJ_DIR)/, $(LIB_SRCS:.cpp=.o))

TEST_NAME	:= run_tests
TEST_SRCS	:= tests/unit/test_main.cpp \
			   tests/unit/config_test.cpp \
			   tests/unit/socket_address_test.cpp \
			   tests/unit/clock_test.cpp \
			   tests/unit/event_loop_test.cpp \
			   tests/unit/header_map_test.cpp \
			   tests/unit/request_parser_test.cpp \
			   tests/unit/router_test.cpp \
			   tests/unit/http_date_test.cpp \
			   tests/unit/path_resolver_test.cpp \
			   tests/unit/response_stream_test.cpp \
			   tests/unit/fs_test.cpp
TEST_OBJS	:= $(addprefix $(OBJ_DIR)/, $(TEST_SRCS:.cpp=.o))

DEPS		:= $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Itests -c $< -o $@

$(TEST_NAME): $(TEST_OBJS) $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(LIB_OBJS) -o $@

test: $(TEST_NAME)
	./$(TEST_NAME)

# Starts the server and talks to it over TCP. WEBSERV_SLOW_TESTS=1 adds the
# tests that wait for timeouts.
integration: $(NAME)
	python3 -m unittest discover -s tests/integration -v

# Requires clang-format; style is in .clang-format.
format:
	clang-format -i $(shell find include src tests -name '*.hpp' -o -name '*.cpp')

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean
	$(MAKE) all

-include $(DEPS)

.PHONY: all clean fclean re test integration format
