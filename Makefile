NAME		:= webserv

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98
CPPFLAGS	:= -Iinclude -MMD -MP

SRC_DIR		:= src
OBJ_DIR		:= obj

CONFIG_SRCS	:= config/ConfigBuilder.cpp \
			   config/ConfigError.cpp \
			   config/ConfigLexer.cpp \
			   config/ConfigLoader.cpp \
			   config/ConfigSyntax.cpp \
			   config/ConfigValues.cpp

LIB_SRCS	:= $(CONFIG_SRCS)
SRCS		:= main.cpp $(LIB_SRCS)

OBJS		:= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))
LIB_OBJS	:= $(addprefix $(OBJ_DIR)/, $(LIB_SRCS:.cpp=.o))

TEST_NAME	:= run_tests
TEST_SRCS	:= tests/unit/test_main.cpp \
			   tests/unit/config_test.cpp
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

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean
	$(MAKE) all

-include $(DEPS)

.PHONY: all clean fclean re test
